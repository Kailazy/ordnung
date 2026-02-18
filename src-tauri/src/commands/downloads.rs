use std::path::PathBuf;

use rusqlite::params;
use tauri::State;
use tauri_plugin_dialog::DialogExt;

use crate::error::AppError;
use crate::models::Download;
use crate::watcher::FolderWatcher;
use crate::AppState;

#[tauri::command]
pub async fn list_downloads(state: State<'_, AppState>) -> Result<Vec<Download>, AppError> {
    let db = state.db.lock().unwrap();

    let mut stmt = db.prepare(
        "SELECT d.id, d.filename, d.filepath, d.extension, d.size_mb, d.detected_at, c.id AS conv_id, c.status AS conv_status, c.output_path AS conv_output, c.size_mb AS conv_size, c.error_msg AS conv_error FROM downloads d LEFT JOIN conversions c ON c.download_id = d.id ORDER BY d.detected_at DESC LIMIT 500",
    )?;

    let rows = stmt
        .query_map([], |row| {
            Ok(Download {
                id: row.get(0)?,
                filename: row.get(1)?,
                filepath: row.get(2)?,
                extension: row.get(3)?,
                size_mb: row.get(4)?,
                detected_at: row.get(5)?,
                conv_id: row.get(6)?,
                conv_status: row.get(7)?,
                conv_output: row.get(8)?,
                conv_size: row.get(9)?,
                conv_error: row.get(10)?,
            })
        })?
        .collect::<Result<Vec<_>, _>>()?;

    Ok(rows)
}

#[tauri::command]
pub async fn delete_download(
    download_id: i64,
    state: State<'_, AppState>,
) -> Result<(), AppError> {
    let db = state.db.lock().unwrap();
    db.execute(
        "DELETE FROM conversions WHERE download_id = ?",
        params![download_id],
    )?;
    db.execute("DELETE FROM downloads WHERE id = ?", params![download_id])?;
    Ok(())
}

#[tauri::command]
pub async fn scan_folder(
    state: State<'_, AppState>,
    app: tauri::AppHandle,
) -> Result<serde_json::Value, AppError> {
    let db = state.db.lock().unwrap();

    let watch_path: Option<String> = db
        .query_row(
            "SELECT value FROM config WHERE key = 'watch_folder'",
            [],
            |row| row.get(0),
        )
        .ok();

    let auto_convert: bool = db
        .query_row(
            "SELECT value FROM config WHERE key = 'auto_convert'",
            [],
            |row| row.get::<_, String>(0),
        )
        .map(|v| v == "1")
        .unwrap_or(true);

    let output_dir: Option<String> = db
        .query_row(
            "SELECT value FROM config WHERE key = 'output_folder'",
            [],
            |row| row.get(0),
        )
        .ok();

    drop(db);

    let path = match watch_path {
        Some(ref p) if !p.is_empty() => p.clone(),
        _ => {
            tracing::warn!("Scan requested but no watch folder configured");
            return Err(AppError::Other("No watch folder configured".to_string()));
        }
    };

    tracing::info!("Scanning folder: {}", path);

    let (scanned, added) = FolderWatcher::scan_existing(
        &PathBuf::from(&path),
        &state.db,
        &state.converter,
        &app,
        auto_convert,
        output_dir,
    )?;

    tracing::info!("Scan complete: {} found, {} new", scanned, added);

    Ok(serde_json::json!({ "scanned": scanned, "added": added }))
}

#[tauri::command]
pub async fn browse_folder(app: tauri::AppHandle) -> Result<String, AppError> {
    let (tx, rx) = std::sync::mpsc::channel::<Option<String>>();

    app.dialog().file().pick_folder(move |folder_path| {
        let path = folder_path.map(|p| p.to_string());
        let _ = tx.send(path);
    });

    match rx.recv() {
        Ok(Some(path)) => Ok(path),
        Ok(None) => Ok(String::new()),
        Err(_) => Ok(String::new()),
    }
}
