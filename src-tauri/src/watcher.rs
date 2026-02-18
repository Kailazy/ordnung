use notify::{Event, EventKind, RecommendedWatcher, RecursiveMode, Watcher};
use rusqlite::{Connection, params};
use std::path::{Path, PathBuf};
use std::sync::{Arc, Mutex};
use tauri::{AppHandle, Emitter};

use crate::converter::ConversionWorker;
use crate::error::AppError;

const AUDIO_EXTENSIONS: &[&str] = &[
    "mp3", "flac", "wav", "aiff", "aif", "ogg", "m4a", "wma", "alac",
];

/// Returns true if the file extension (without dot) is an audio format we handle.
fn is_audio_file(path: &Path) -> bool {
    path.extension()
        .and_then(|e| e.to_str())
        .map(|e| AUDIO_EXTENSIONS.contains(&e.to_lowercase().as_str()))
        .unwrap_or(false)
}

/// File system watcher for a configured download folder.
pub struct FolderWatcher {
    pub handle: Option<RecommendedWatcher>,
    pub watch_path: Option<PathBuf>,
}

impl FolderWatcher {
    pub fn new() -> Self {
        FolderWatcher {
            handle: None,
            watch_path: None,
        }
    }

    /// Start watching the given directory for new audio files.
    /// On Create events: wait for file size stabilization, then insert into downloads and
    /// optionally auto-convert.
    pub fn start(
        &mut self,
        path: &Path,
        db: Arc<Mutex<Connection>>,
        converter: Arc<ConversionWorker>,
        app: AppHandle,
    ) -> Result<(), AppError> {
        // Stop any existing watcher first
        self.stop();

        if !path.is_dir() {
            return Err(AppError::Other(format!(
                "Not a valid directory: {}",
                path.display()
            )));
        }

        let watch_path = path.to_path_buf();
        let (tx, rx) = std::sync::mpsc::channel::<notify::Result<Event>>();

        let mut watcher = notify::recommended_watcher(tx)
            .map_err(|e| AppError::Other(format!("Failed to create watcher: {}", e)))?;

        watcher
            .watch(path, RecursiveMode::NonRecursive)
            .map_err(|e| AppError::Other(format!("Failed to watch path: {}", e)))?;

        self.handle = Some(watcher);
        self.watch_path = Some(watch_path);

        // Spawn a thread to process watcher events
        std::thread::spawn(move || {
            for result in rx {
                match result {
                    Ok(event) => {
                        if matches!(event.kind, EventKind::Create(_)) {
                            for path in &event.paths {
                                if path.is_file() && is_audio_file(path) {
                                    let path = path.clone();
                                    let db = db.clone();
                                    let converter = converter.clone();
                                    let app = app.clone();

                                    // Spawn a thread for file stabilization so we don't block the event loop
                                    std::thread::spawn(move || {
                                        handle_new_file(&path, &db, &converter, &app);
                                    });
                                }
                            }
                        }
                    }
                    Err(e) => {
                        tracing::warn!("Watcher error: {}", e);
                    }
                }
            }
            tracing::info!("Watcher event loop exited");
        });

        Ok(())
    }

    /// Stop the folder watcher.
    pub fn stop(&mut self) {
        self.handle = None;
        self.watch_path = None;
    }

    /// Check if the watcher is active.
    pub fn is_running(&self) -> bool {
        self.handle.is_some()
    }

    /// One-time scan of existing audio files in a folder.
    /// Inserts new files into the downloads table and optionally auto-converts them.
    /// Returns (scanned, added).
    pub fn scan_existing(
        path: &Path,
        db: &Arc<Mutex<Connection>>,
        converter: &Arc<ConversionWorker>,
        app: &AppHandle,
        auto_convert: bool,
        output_dir: Option<String>,
    ) -> Result<(usize, usize), AppError> {
        if !path.is_dir() {
            return Ok((0, 0));
        }

        let entries = std::fs::read_dir(path)?;
        let mut scanned = 0usize;
        let mut added = 0usize;
        let mut to_convert: Vec<(i64, PathBuf, String)> = Vec::new();

        let conn = db.lock().unwrap();

        for entry in entries.flatten() {
            let fpath = entry.path();
            if !fpath.is_file() || !is_audio_file(&fpath) {
                continue;
            }

            scanned += 1;
            let filepath_str = fpath.to_string_lossy().to_string();

            // Check if already in DB
            let exists: bool = conn
                .query_row(
                    "SELECT 1 FROM downloads WHERE filepath = ?",
                    params![filepath_str],
                    |_| Ok(()),
                )
                .is_ok();

            if exists {
                continue;
            }

            let filename = fpath
                .file_name()
                .unwrap_or_default()
                .to_string_lossy()
                .to_string();
            let extension = fpath
                .extension()
                .unwrap_or_default()
                .to_string_lossy()
                .to_lowercase();
            let size_mb = std::fs::metadata(&fpath)
                .map(|m| (m.len() as f64) / (1024.0 * 1024.0))
                .map(|s| (s * 100.0).round() / 100.0)
                .unwrap_or(0.0);

            let mut stmt = conn.prepare(
                "INSERT INTO downloads (filename, filepath, extension, size_mb) VALUES (?, ?, ?, ?)",
            ).map_err(AppError::Database)?;
            let download_id = stmt
                .insert(params![filename, filepath_str, extension, size_mb])
                .map_err(AppError::Database)?;

            added += 1;
            tracing::debug!("New file detected: {} ({})", filename, extension);

            let _ = app.emit(
                "download-detected",
                serde_json::json!({ "id": download_id, "filename": filename }),
            );

            if auto_convert {
                to_convert.push((download_id, fpath.clone(), extension));
            }
        }

        drop(conn);

        // Auto-convert if configured
        if let Some(ref out_dir) = output_dir {
            let out_path = PathBuf::from(out_dir);
            for (download_id, fpath, ext) in to_convert {
                let _ = converter.enqueue(db, download_id, fpath, ext, out_path.clone());
            }
        }

        Ok((scanned, added))
    }
}

/// Handle a new audio file detected by the watcher: wait for stabilization, insert to DB, auto-convert.
fn handle_new_file(
    path: &Path,
    db: &Arc<Mutex<Connection>>,
    converter: &Arc<ConversionWorker>,
    app: &AppHandle,
) {
    tracing::debug!(
        "File created event: {}",
        path.display()
    );

    // Wait for file size stabilization
    if !wait_for_file_ready(path, 30) {
        tracing::warn!("File never stabilized: {}", path.display());
        return;
    }

    let filename = path
        .file_name()
        .unwrap_or_default()
        .to_string_lossy()
        .to_string();
    let filepath_str = path.to_string_lossy().to_string();
    let extension = path
        .extension()
        .unwrap_or_default()
        .to_string_lossy()
        .to_lowercase();
    let size_mb = std::fs::metadata(path)
        .map(|m| (m.len() as f64) / (1024.0 * 1024.0))
        .map(|s| (s * 100.0).round() / 100.0)
        .unwrap_or(0.0);

    tracing::info!(
        "Watcher detected: {} ({:.1} MB, {})",
        filename,
        size_mb,
        extension
    );

    let conn = db.lock().unwrap();

    // Check if already in DB
    let exists: bool = conn
        .query_row(
            "SELECT 1 FROM downloads WHERE filepath = ?",
            params![filepath_str],
            |_| Ok(()),
        )
        .is_ok();

    if exists {
        tracing::debug!("Already in DB, skipping: {}", filename);
        return;
    }

    let insert_result = conn.execute(
        "INSERT INTO downloads (filename, filepath, extension, size_mb) VALUES (?, ?, ?, ?)",
        params![filename, filepath_str, extension, size_mb],
    );

    let download_id = match insert_result {
        Ok(_) => conn.last_insert_rowid(),
        Err(e) => {
            tracing::error!("Error inserting download {}: {}", filename, e);
            return;
        }
    };

    drop(conn);

    tracing::debug!("Added to downloads (id={}): {}", download_id, filename);

    let _ = app.emit(
        "download-detected",
        serde_json::json!({ "id": download_id, "filename": filename }),
    );

    // Auto-convert if enabled
    maybe_auto_convert(db, converter, download_id, path.to_path_buf(), extension);
}

/// Wait until the file size stabilizes (download complete).
/// Polls every 500ms, requires 3 consecutive stable reads.
fn wait_for_file_ready(path: &Path, timeout_secs: u64) -> bool {
    let start = std::time::Instant::now();
    let timeout = std::time::Duration::from_secs(timeout_secs);
    let mut prev_size: i64 = -1;
    let mut stable_count = 0u32;

    while start.elapsed() < timeout {
        let size = match std::fs::metadata(path) {
            Ok(m) => m.len() as i64,
            Err(_) => return false,
        };

        if size == prev_size && size > 0 {
            stable_count += 1;
            if stable_count >= 3 {
                return true;
            }
        } else {
            stable_count = 0;
        }

        prev_size = size;
        std::thread::sleep(std::time::Duration::from_millis(500));
    }

    prev_size > 0
}

/// Check config and auto-convert if enabled.
fn maybe_auto_convert(
    db: &Arc<Mutex<Connection>>,
    converter: &Arc<ConversionWorker>,
    download_id: i64,
    filepath: PathBuf,
    extension: String,
) {
    let conn = db.lock().unwrap();

    let auto_convert: bool = conn
        .query_row(
            "SELECT value FROM config WHERE key = 'auto_convert'",
            [],
            |row| row.get::<_, String>(0),
        )
        .map(|v| v == "1")
        .unwrap_or(true); // Default to true, matching Python behavior

    if !auto_convert {
        return;
    }

    let output_dir: Option<String> = conn
        .query_row(
            "SELECT value FROM config WHERE key = 'output_folder'",
            [],
            |row| row.get(0),
        )
        .ok();

    drop(conn);

    if let Some(out_dir) = output_dir {
        if !out_dir.is_empty() {
            let _ = converter.enqueue(db, download_id, filepath, extension, PathBuf::from(out_dir));
        }
    }
}
