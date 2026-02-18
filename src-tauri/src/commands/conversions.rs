use std::path::PathBuf;

use rusqlite::params;
use tauri::State;

use crate::error::AppError;
use crate::models::{Conversion, ConversionStats};
use crate::AppState;

#[tauri::command]
pub async fn list_conversions(state: State<'_, AppState>) -> Result<Vec<Conversion>, AppError> {
    let db = state.db.lock().unwrap();

    let mut stmt =
        db.prepare("SELECT id, download_id, source_path, output_path, source_ext, status, error_msg, size_mb, started_at, finished_at, created_at FROM conversions ORDER BY created_at DESC LIMIT 500")?;

    let rows = stmt
        .query_map([], |row| {
            Ok(Conversion {
                id: row.get(0)?,
                download_id: row.get(1)?,
                source_path: row.get(2)?,
                output_path: row.get(3)?,
                source_ext: row.get(4)?,
                status: row.get(5)?,
                error_msg: row.get(6)?,
                size_mb: row.get(7)?,
                started_at: row.get(8)?,
                finished_at: row.get(9)?,
                created_at: row.get(10)?,
            })
        })?
        .collect::<Result<Vec<_>, _>>()?;

    Ok(rows)
}

#[tauri::command]
pub async fn conversion_stats(state: State<'_, AppState>) -> Result<ConversionStats, AppError> {
    let db = state.db.lock().unwrap();

    let mut stmt =
        db.prepare("SELECT status, COUNT(*) as count FROM conversions GROUP BY status")?;

    let rows: Vec<(String, i64)> = stmt
        .query_map([], |row| {
            Ok((row.get::<_, String>(0)?, row.get::<_, i64>(1)?))
        })?
        .collect::<Result<Vec<_>, _>>()?;

    let mut stats = ConversionStats {
        pending: 0,
        converting: 0,
        done: 0,
        failed: 0,
        skipped: 0,
        queue_size: state.converter.get_queue_size(),
        worker_running: state.converter.is_running(),
    };

    for (status, count) in rows {
        match status.as_str() {
            "pending" => stats.pending = count,
            "converting" => stats.converting = count,
            "done" => stats.done = count,
            "failed" => stats.failed = count,
            "skipped" => stats.skipped = count,
            _ => {}
        }
    }

    Ok(stats)
}

#[tauri::command]
pub async fn retry_conversion(
    conversion_id: i64,
    state: State<'_, AppState>,
) -> Result<(), AppError> {
    let db = state.db.lock().unwrap();

    let row: Option<(String, String)> = db
        .query_row(
            "SELECT status, source_path FROM conversions WHERE id = ?",
            params![conversion_id],
            |row| Ok((row.get::<_, String>(0)?, row.get::<_, String>(1)?)),
        )
        .ok();

    let (status, source_path) = match row {
        Some(r) => r,
        None => return Err(AppError::Other("Conversion not found".to_string())),
    };

    if status != "failed" {
        return Err(AppError::Other(
            "Can only retry failed conversions".to_string(),
        ));
    }

    let output_dir: Option<String> = db
        .query_row(
            "SELECT value FROM config WHERE key = 'output_folder'",
            [],
            |row| row.get(0),
        )
        .ok();

    let output_dir = match output_dir {
        Some(ref d) if !d.is_empty() => d.clone(),
        _ => return Err(AppError::Other("No output folder configured".to_string())),
    };

    db.execute(
        "UPDATE conversions SET status='pending', error_msg='', started_at=NULL, finished_at=NULL WHERE id=?",
        params![conversion_id],
    )?;

    drop(db);

    state.converter.reenqueue(
        conversion_id,
        PathBuf::from(source_path),
        PathBuf::from(output_dir),
    );

    Ok(())
}

#[tauri::command]
pub async fn convert_single(
    download_id: i64,
    state: State<'_, AppState>,
) -> Result<serde_json::Value, AppError> {
    let db = state.db.lock().unwrap();

    let row: Option<(String, String)> = db
        .query_row(
            "SELECT filepath, extension FROM downloads WHERE id = ?",
            params![download_id],
            |row| Ok((row.get::<_, String>(0)?, row.get::<_, String>(1)?)),
        )
        .ok();

    let (filepath, extension) = match row {
        Some(r) => r,
        None => return Err(AppError::Other("Download not found".to_string())),
    };

    let output_dir: Option<String> = db
        .query_row(
            "SELECT value FROM config WHERE key = 'output_folder'",
            [],
            |row| row.get(0),
        )
        .ok();

    let output_dir = match output_dir {
        Some(ref d) if !d.is_empty() => d.clone(),
        _ => return Err(AppError::Other("No output folder configured".to_string())),
    };

    drop(db);

    let conv_id = state.converter.enqueue(
        &state.db,
        download_id,
        PathBuf::from(filepath),
        extension,
        PathBuf::from(output_dir),
    )?;

    Ok(serde_json::json!({ "ok": true, "conversion_id": conv_id }))
}
