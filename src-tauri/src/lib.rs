use std::path::PathBuf;
use std::sync::{Arc, Mutex};

use rusqlite::Connection;
use tauri::{AppHandle, Manager};

use crate::converter::ConversionWorker;
use crate::watcher::FolderWatcher;

pub mod commands;
pub mod converter;
pub mod db;
pub mod error;
pub mod importer;
pub mod models;
pub mod watcher;

/// Shared application state, managed by Tauri.
pub struct AppState {
    pub db: Arc<Mutex<Connection>>,
    pub converter: Arc<ConversionWorker>,
    pub watcher: Arc<Mutex<FolderWatcher>>,
}

#[cfg_attr(mobile, tauri::mobile_entry_point)]
pub fn run() {
    // Initialize tracing subscriber for structured logging
    tracing_subscriber::fmt()
        .with_env_filter(
            tracing_subscriber::EnvFilter::try_from_default_env()
                .unwrap_or_else(|_| tracing_subscriber::EnvFilter::new("info")),
        )
        .init();

    tauri::Builder::default()
        .plugin(tauri_plugin_dialog::init())
        .plugin(tauri_plugin_shell::init())
        .plugin(tauri_plugin_fs::init())
        .setup(|app| {
            let data_dir = app
                .path()
                .app_data_dir()
                .unwrap_or_else(|_| PathBuf::from("."));

            std::fs::create_dir_all(&data_dir).ok();

            tracing::info!("========================================");
            tracing::info!("Eyebags Terminal starting");
            tracing::info!("Data directory: {}", data_dir.display());
            tracing::info!("========================================");

            // Open database
            let conn = db::open_db(&data_dir)
                .expect("Failed to open database");
            let db = Arc::new(Mutex::new(conn));

            // Start conversion worker
            let converter = Arc::new(ConversionWorker::start(db.clone(), app.handle().clone()));

            // Create folder watcher
            let watcher = Arc::new(Mutex::new(FolderWatcher::new()));

            let state = AppState {
                db: db.clone(),
                converter: converter.clone(),
                watcher: watcher.clone(),
            };

            // Restore state from DB on startup
            restore_on_startup(&db, &converter, &watcher, app.handle());

            app.manage(state);

            Ok(())
        })
        .invoke_handler(tauri::generate_handler![
            commands::playlists::list_playlists,
            commands::playlists::import_playlist,
            commands::playlists::delete_playlist,
            commands::playlists::get_tracks,
            commands::playlists::export_tracks,
            commands::songs::update_song_format,
            commands::songs::update_song_aiff,
            commands::songs::bulk_update_format,
            commands::songs::get_song_playlists,
            commands::songs::update_song_playlists,
            commands::downloads::list_downloads,
            commands::downloads::delete_download,
            commands::downloads::scan_folder,
            commands::downloads::browse_folder,
            commands::conversions::list_conversions,
            commands::conversions::conversion_stats,
            commands::conversions::retry_conversion,
            commands::conversions::convert_single,
            commands::config::get_watch_config,
            commands::config::set_watch_config,
        ])
        .run(tauri::generate_context!())
        .expect("error while running tauri application");
}

/// Restore watcher and re-queue interrupted conversions on startup.
fn restore_on_startup(
    db: &Arc<Mutex<Connection>>,
    converter: &Arc<ConversionWorker>,
    watcher: &Arc<Mutex<FolderWatcher>>,
    app: &AppHandle,
) {
    tracing::info!("Restoring state from database...");

    let conn = db.lock().unwrap();

    // Restore watcher if watch_folder is configured
    let watch_path: Option<String> = conn
        .query_row(
            "SELECT value FROM config WHERE key = 'watch_folder'",
            [],
            |row| row.get(0),
        )
        .ok();

    if let Some(ref wp) = watch_path {
        if !wp.is_empty() {
            let path = PathBuf::from(wp);
            if path.is_dir() {
                let mut w = watcher.lock().unwrap();
                match w.start(&path, db.clone(), converter.clone(), app.clone()) {
                    Ok(()) => tracing::info!("Folder watcher restored: {}", wp),
                    Err(e) => tracing::error!("Failed to restore watcher on {}: {}", wp, e),
                }
            } else {
                tracing::warn!("Watch folder no longer exists: {}", wp);
            }
        }
    } else {
        tracing::info!("No watch folder to restore");
    }

    // Re-queue pending/converting conversions
    let output_dir: Option<String> = conn
        .query_row(
            "SELECT value FROM config WHERE key = 'output_folder'",
            [],
            |row| row.get(0),
        )
        .ok();

    match output_dir {
        Some(ref od) if !od.is_empty() => {
            let mut stmt = conn
                .prepare(
                    "SELECT id, source_path FROM conversions WHERE status IN ('pending','converting')",
                )
                .ok();

            if let Some(ref mut s) = stmt {
                let pending: Vec<(i64, String)> = s
                    .query_map([], |row| {
                        Ok((row.get::<_, i64>(0)?, row.get::<_, String>(1)?))
                    })
                    .ok()
                    .map(|rows| rows.filter_map(|r| r.ok()).collect())
                    .unwrap_or_default();

                if !pending.is_empty() {
                    tracing::info!("Re-queuing {} interrupted conversions", pending.len());

                    for (id, source_path) in &pending {
                        let _ = conn.execute(
                            "UPDATE conversions SET status='pending' WHERE id=?",
                            rusqlite::params![id],
                        );
                        converter.reenqueue(
                            *id,
                            PathBuf::from(source_path),
                            PathBuf::from(od),
                        );
                    }
                }
            }
        }
        _ => {
            tracing::info!("No output folder configured, converter not started");
        }
    }
}
