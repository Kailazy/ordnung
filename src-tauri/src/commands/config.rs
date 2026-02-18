use std::path::PathBuf;

use rusqlite::params;
use tauri::State;

use crate::error::AppError;
use crate::models::WatchConfig;
use crate::AppState;

#[tauri::command]
pub async fn get_watch_config(state: State<'_, AppState>) -> Result<WatchConfig, AppError> {
    let db = state.db.lock().unwrap();

    let watch_path: String = db
        .query_row(
            "SELECT value FROM config WHERE key = 'watch_folder'",
            [],
            |row| row.get(0),
        )
        .unwrap_or_default();

    let output_folder: String = db
        .query_row(
            "SELECT value FROM config WHERE key = 'output_folder'",
            [],
            |row| row.get(0),
        )
        .unwrap_or_default();

    let auto_convert: bool = db
        .query_row(
            "SELECT value FROM config WHERE key = 'auto_convert'",
            [],
            |row| row.get::<_, String>(0),
        )
        .map(|v| v == "1")
        .unwrap_or(true);

    let watcher = state.watcher.lock().unwrap();

    Ok(WatchConfig {
        path: watch_path,
        output_folder,
        auto_convert,
        active: watcher.is_running(),
        converter_running: state.converter.is_running(),
        queue_size: state.converter.get_queue_size(),
    })
}

#[tauri::command]
pub async fn set_watch_config(
    path: Option<String>,
    output_folder: Option<String>,
    auto_convert: Option<bool>,
    state: State<'_, AppState>,
    app: tauri::AppHandle,
) -> Result<WatchConfig, AppError> {
    {
        let db = state.db.lock().unwrap();

        if let Some(ref p) = path {
            let p = p.trim();
            if p.is_empty() {
                // Stop watcher and remove config
                let mut watcher = state.watcher.lock().unwrap();
                watcher.stop();
                db.execute("DELETE FROM config WHERE key = 'watch_folder'", [])?;
            } else {
                let path_buf = PathBuf::from(p);
                if !path_buf.is_dir() {
                    return Err(AppError::Other(format!(
                        "Source directory not found: {}",
                        p
                    )));
                }
                db.execute(
                    "INSERT OR REPLACE INTO config (key, value) VALUES ('watch_folder', ?)",
                    params![p],
                )?;
            }
        }

        if let Some(ref of) = output_folder {
            let of = of.trim();
            if of.is_empty() {
                db.execute("DELETE FROM config WHERE key = 'output_folder'", [])?;
            } else {
                std::fs::create_dir_all(of)?;
                db.execute(
                    "INSERT OR REPLACE INTO config (key, value) VALUES ('output_folder', ?)",
                    params![of],
                )?;
            }
        }

        if let Some(ac) = auto_convert {
            let val = if ac { "1" } else { "0" };
            db.execute(
                "INSERT OR REPLACE INTO config (key, value) VALUES ('auto_convert', ?)",
                params![val],
            )?;
        }
    }

    tracing::info!(
        "Config saved: watch={:?} output={:?} auto_convert={:?}",
        path,
        output_folder,
        auto_convert
    );

    // Restart services based on new config
    restart_services(&state, &app)?;

    // Read back current state
    let db = state.db.lock().unwrap();

    let watch_path: String = db
        .query_row(
            "SELECT value FROM config WHERE key = 'watch_folder'",
            [],
            |row| row.get(0),
        )
        .unwrap_or_default();

    let out_folder: String = db
        .query_row(
            "SELECT value FROM config WHERE key = 'output_folder'",
            [],
            |row| row.get(0),
        )
        .unwrap_or_default();

    let ac: bool = db
        .query_row(
            "SELECT value FROM config WHERE key = 'auto_convert'",
            [],
            |row| row.get::<_, String>(0),
        )
        .map(|v| v == "1")
        .unwrap_or(true);

    let watcher = state.watcher.lock().unwrap();

    Ok(WatchConfig {
        path: watch_path,
        output_folder: out_folder,
        auto_convert: ac,
        active: watcher.is_running(),
        converter_running: state.converter.is_running(),
        queue_size: state.converter.get_queue_size(),
    })
}

/// Restart watcher based on current DB config.
fn restart_services(state: &AppState, app: &tauri::AppHandle) -> Result<(), AppError> {
    let db = state.db.lock().unwrap();

    let watch_path: Option<String> = db
        .query_row(
            "SELECT value FROM config WHERE key = 'watch_folder'",
            [],
            |row| row.get(0),
        )
        .ok();

    drop(db);

    let mut watcher = state.watcher.lock().unwrap();

    match watch_path {
        Some(ref p) if !p.is_empty() && PathBuf::from(p).is_dir() => {
            watcher.start(
                &PathBuf::from(p),
                state.db.clone(),
                state.converter.clone(),
                app.clone(),
            )?;
            tracing::info!("Folder watcher restarted: {}", p);
        }
        _ => {
            watcher.stop();
        }
    }

    Ok(())
}
