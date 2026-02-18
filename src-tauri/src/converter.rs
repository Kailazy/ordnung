use std::path::{Path, PathBuf};
use std::sync::atomic::{AtomicBool, AtomicUsize, Ordering};
use std::sync::{Arc, Mutex};

use rusqlite::{Connection, params};
use tauri::{AppHandle, Emitter};
use tokio::sync::mpsc;

use crate::error::AppError;

/// A single conversion job to be processed by the worker.
pub struct ConvJob {
    pub conv_id: i64,
    pub source_path: PathBuf,
    pub output_dir: PathBuf,
}

/// Background worker that processes audio-to-AIFF conversion jobs.
pub struct ConversionWorker {
    pub sender: mpsc::Sender<ConvJob>,
    pub queue_size: Arc<AtomicUsize>,
    pub running: Arc<AtomicBool>,
}

impl ConversionWorker {
    /// Spawn the background conversion worker.
    /// Returns immediately; the worker processes jobs from the channel in a tokio task.
    pub fn start(db: Arc<Mutex<Connection>>, app: AppHandle) -> Self {
        let (tx, mut rx) = mpsc::channel::<ConvJob>(256);
        let queue_size = Arc::new(AtomicUsize::new(0));
        let running = Arc::new(AtomicBool::new(true));

        let qs = queue_size.clone();
        let r = running.clone();

        tokio::spawn(async move {
            let ffmpeg_bin = find_ffmpeg().await;
            if ffmpeg_bin.is_none() {
                tracing::error!("ffmpeg not found on this system");
            }

            while r.load(Ordering::Relaxed) {
                match rx.recv().await {
                    Some(job) => {
                        process_job(&db, &app, &ffmpeg_bin, job).await;
                        qs.fetch_sub(1, Ordering::Relaxed);
                    }
                    None => break,
                }
            }

            tracing::info!("Conversion worker shut down");
        });

        ConversionWorker {
            sender: tx,
            queue_size,
            running,
        }
    }

    /// Enqueue a conversion job. Creates the DB record and sends the job to the worker.
    /// Returns the conversion ID (existing or newly created).
    pub fn enqueue(
        &self,
        db: &Arc<Mutex<Connection>>,
        download_id: i64,
        source_path: PathBuf,
        source_ext: String,
        output_dir: PathBuf,
    ) -> Result<i64, AppError> {
        let conn = db.lock().unwrap();
        let source_str = source_path.to_string_lossy().to_string();

        // Check for existing pending/converting/done conversion (dedup)
        let existing: Option<i64> = conn
            .query_row(
                "SELECT id FROM conversions WHERE source_path = ? AND status IN ('pending','converting','done')",
                params![source_str],
                |row| row.get(0),
            )
            .ok();

        if let Some(id) = existing {
            tracing::debug!(
                "Conversion already exists (id={}) for {}, skipping",
                id,
                source_path.file_name().unwrap_or_default().to_string_lossy()
            );
            return Ok(id);
        }

        let mut stmt = conn.prepare(
            "INSERT INTO conversions (download_id, source_path, source_ext, status) VALUES (?, ?, ?, 'pending')",
        )?;
        let conv_id = stmt.insert(params![download_id, source_str, source_ext])?;
        drop(conn);

        tracing::info!(
            "Queued conversion #{}: {} -> aiff (output: {})",
            conv_id,
            source_path.file_name().unwrap_or_default().to_string_lossy(),
            output_dir.display()
        );

        self.queue_size.fetch_add(1, Ordering::Relaxed);
        let _ = self.sender.try_send(ConvJob {
            conv_id,
            source_path,
            output_dir,
        });

        Ok(conv_id)
    }

    /// Re-enqueue an existing conversion record (for retries or startup restore).
    pub fn reenqueue(&self, conv_id: i64, source_path: PathBuf, output_dir: PathBuf) {
        self.queue_size.fetch_add(1, Ordering::Relaxed);
        let _ = self.sender.try_send(ConvJob {
            conv_id,
            source_path,
            output_dir,
        });
    }

    /// Check whether the worker task is still running.
    pub fn is_running(&self) -> bool {
        self.running.load(Ordering::Relaxed)
    }

    /// Get the number of jobs currently in the queue.
    pub fn get_queue_size(&self) -> usize {
        self.queue_size.load(Ordering::Relaxed)
    }
}

/// Process a single conversion job: update DB, run FFmpeg, move source file.
async fn process_job(
    db: &Arc<Mutex<Connection>>,
    app: &AppHandle,
    ffmpeg_bin: &Option<PathBuf>,
    job: ConvJob,
) {
    let fname = job
        .source_path
        .file_name()
        .unwrap_or_default()
        .to_string_lossy()
        .to_string();

    tracing::info!("Converting #{}: {}", job.conv_id, fname);

    // Mark as converting
    {
        let conn = db.lock().unwrap();
        let _ = conn.execute(
            "UPDATE conversions SET status='converting', started_at=CURRENT_TIMESTAMP WHERE id=?",
            params![job.conv_id],
        );
    }
    emit_conversion_update(app, job.conv_id, "converting");

    let (output_path, error) =
        convert_to_aiff(&job.source_path, &job.output_dir, ffmpeg_bin).await;

    if !error.is_empty() {
        tracing::error!("Conversion #{} FAILED: {} -- {}", job.conv_id, fname, &error[..error.len().min(200)]);
        let conn = db.lock().unwrap();
        let _ = conn.execute(
            "UPDATE conversions SET status='failed', error_msg=?, finished_at=CURRENT_TIMESTAMP WHERE id=?",
            params![&error[..error.len().min(1000)], job.conv_id],
        );
        emit_conversion_update(app, job.conv_id, "failed");
    } else {
        let size_mb = std::fs::metadata(&output_path)
            .map(|m| (m.len() as f64) / (1024.0 * 1024.0))
            .map(|s| (s * 100.0).round() / 100.0)
            .unwrap_or(0.0);

        let out_fname = Path::new(&output_path)
            .file_name()
            .unwrap_or_default()
            .to_string_lossy()
            .to_string();

        tracing::info!(
            "Conversion #{} DONE: {} -> {} ({:.1} MB)",
            job.conv_id,
            fname,
            out_fname,
            size_mb
        );

        {
            let conn = db.lock().unwrap();
            let _ = conn.execute(
                "UPDATE conversions SET status='done', output_path=?, size_mb=?, finished_at=CURRENT_TIMESTAMP WHERE id=?",
                params![output_path, size_mb, job.conv_id],
            );
        }

        // Move source file to _converted/ subfolder
        if let Some(source_dir) = job.source_path.parent() {
            let converted_dir = source_dir.join("_converted");
            if std::fs::create_dir_all(&converted_dir).is_ok() {
                let dest = converted_dir.join(&fname);
                if job.source_path.is_file() {
                    match std::fs::rename(&job.source_path, &dest) {
                        Ok(()) => {
                            let conn = db.lock().unwrap();
                            let source_str = job.source_path.to_string_lossy().to_string();
                            let dest_str = dest.to_string_lossy().to_string();
                            let _ = conn.execute(
                                "UPDATE downloads SET filepath = ? WHERE filepath = ?",
                                params![dest_str, source_str],
                            );
                            let _ = conn.execute(
                                "UPDATE conversions SET source_path = ? WHERE id = ?",
                                params![dest_str, job.conv_id],
                            );
                            tracing::info!("Moved source to _converted: {}", fname);
                        }
                        Err(e) => {
                            tracing::warn!("Could not move source to _converted: {}", e);
                        }
                    }
                }
            }
        }

        emit_conversion_update(app, job.conv_id, "done");
    }
}

/// Convert an audio file to AIFF format.
/// Returns (output_path, error_msg). On success error_msg is empty.
async fn convert_to_aiff(
    source_path: &Path,
    output_dir: &Path,
    ffmpeg_bin: &Option<PathBuf>,
) -> (String, String) {
    let ffmpeg = match ffmpeg_bin {
        Some(f) => f.clone(),
        None => return (String::new(), "ffmpeg not found".to_string()),
    };

    if !source_path.is_file() {
        return (
            String::new(),
            format!("source file not found: {}", source_path.display()),
        );
    }

    if std::fs::create_dir_all(output_dir).is_err() {
        return (
            String::new(),
            format!("cannot create output directory: {}", output_dir.display()),
        );
    }

    let stem = source_path
        .file_stem()
        .unwrap_or_default()
        .to_string_lossy()
        .to_string();

    let mut output_path = output_dir.join(format!("{}.aiff", stem));

    // Avoid overwriting: add _1, _2 suffix
    let mut counter = 1u32;
    while output_path.exists() {
        output_path = output_dir.join(format!("{}_{}.aiff", stem, counter));
        counter += 1;
    }

    let src_ext = source_path
        .extension()
        .unwrap_or_default()
        .to_string_lossy()
        .to_lowercase();

    // Already AIFF: just copy
    if src_ext == "aiff" || src_ext == "aif" {
        match std::fs::copy(source_path, &output_path) {
            Ok(_) => return (output_path.to_string_lossy().to_string(), String::new()),
            Err(e) => return (String::new(), e.to_string()),
        }
    }

    // FFmpeg conversion: lossless PCM into AIFF container
    let result = tokio::process::Command::new(&ffmpeg)
        .args([
            "-i",
            &source_path.to_string_lossy(),
            "-c:a",
            "pcm_s16be",
            "-f",
            "aiff",
            "-y",
            &output_path.to_string_lossy(),
        ])
        .output()
        .await;

    match result {
        Ok(output) => {
            if !output.status.success() {
                let stderr = String::from_utf8_lossy(&output.stderr);
                let truncated = if stderr.len() > 500 {
                    &stderr[stderr.len() - 500..]
                } else {
                    &stderr
                };
                let _ = std::fs::remove_file(&output_path);
                (String::new(), truncated.to_string())
            } else {
                (output_path.to_string_lossy().to_string(), String::new())
            }
        }
        Err(e) => {
            let _ = std::fs::remove_file(&output_path);
            (String::new(), e.to_string())
        }
    }
}

/// Locate the ffmpeg binary, checking common install paths on Windows.
pub async fn find_ffmpeg() -> Option<PathBuf> {
    let candidates: Vec<PathBuf> = {
        let mut c = vec![PathBuf::from("ffmpeg")];

        #[cfg(target_os = "windows")]
        {
            c.push(PathBuf::from(
                r"C:\ProgramData\chocolatey\bin\ffmpeg.exe",
            ));
            if let Ok(local) = std::env::var("LOCALAPPDATA") {
                c.push(PathBuf::from(format!(
                    r"{}\Microsoft\WinGet\Links\ffmpeg.exe",
                    local
                )));
            }
        }

        c
    };

    for candidate in candidates {
        let result = tokio::process::Command::new(&candidate)
            .arg("-version")
            .stdout(std::process::Stdio::null())
            .stderr(std::process::Stdio::null())
            .status()
            .await;

        if let Ok(status) = result {
            if status.success() {
                tracing::info!("Found ffmpeg at: {}", candidate.display());
                return Some(candidate);
            }
        }
    }

    None
}

/// Emit a conversion-update event to the frontend.
fn emit_conversion_update(app: &AppHandle, conv_id: i64, status: &str) {
    let _ = app.emit(
        "conversion-update",
        serde_json::json!({ "id": conv_id, "status": status }),
    );
}
