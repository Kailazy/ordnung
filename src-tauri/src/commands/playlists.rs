use std::collections::HashMap;

use rusqlite::params;
use tauri::State;

use crate::db::make_match_key;
use crate::error::AppError;
use crate::importer::parse_rekordbox_txt;
use crate::models::{Playlist, PlaylistTrack, SongPlaylist};
use crate::AppState;

#[tauri::command]
pub async fn list_playlists(state: State<'_, AppState>) -> Result<Vec<Playlist>, AppError> {
    let db = state.db.lock().unwrap();

    let mut stmt = db.prepare(
        "SELECT id, name, imported_at FROM playlists ORDER BY imported_at DESC",
    )?;

    let playlist_rows: Vec<(i64, String, String)> = stmt
        .query_map([], |row| {
            Ok((
                row.get::<_, i64>(0)?,
                row.get::<_, String>(1)?,
                row.get::<_, String>(2)?,
            ))
        })?
        .collect::<Result<Vec<_>, _>>()?;

    let mut result = Vec::with_capacity(playlist_rows.len());

    for (id, name, imported_at) in playlist_rows {
        let mut count_stmt = db.prepare(
            "SELECT s.format, COUNT(*) as count FROM playlist_songs ps JOIN songs s ON s.id = ps.song_id WHERE ps.playlist_id = ? GROUP BY s.format",
        )?;

        let format_counts_rows: Vec<(String, i64)> = count_stmt
            .query_map(params![id], |row| {
                Ok((row.get::<_, String>(0)?, row.get::<_, i64>(1)?))
            })?
            .collect::<Result<Vec<_>, _>>()?;

        let total: i64 = format_counts_rows.iter().map(|(_, c)| c).sum();
        let format_map: HashMap<String, i64> = format_counts_rows.into_iter().collect();
        let format_counts = serde_json::to_value(&format_map).unwrap_or(serde_json::json!({}));

        result.push(Playlist {
            id,
            name,
            imported_at,
            total,
            format_counts,
        });
    }

    Ok(result)
}

#[tauri::command]
pub async fn import_playlist(
    file_path: String,
    name: Option<String>,
    state: State<'_, AppState>,
) -> Result<serde_json::Value, AppError> {
    let path = std::path::PathBuf::from(&file_path);

    // Derive playlist name from filename if not provided
    let playlist_name = match name {
        Some(n) if !n.trim().is_empty() => n.trim().to_string(),
        _ => path
            .file_stem()
            .unwrap_or_default()
            .to_string_lossy()
            .to_string(),
    };

    let tracks = parse_rekordbox_txt(&path)?;
    if tracks.is_empty() {
        return Err(AppError::Other("No tracks found in file".to_string()));
    }

    let db = state.db.lock().unwrap();

    let mut insert_playlist = db.prepare("INSERT INTO playlists (name) VALUES (?)")?;
    let playlist_id = insert_playlist.insert(params![playlist_name])?;

    let mut added = 0i64;
    let track_count = tracks.len();

    for t in &tracks {
        let mk = make_match_key(&t.artist, &t.title);

        db.execute(
            "INSERT OR IGNORE INTO songs (title, artist, album, genre, bpm, rating, time, key_sig, date_added, format, has_aiff, match_key) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, 'mp3', 0, ?)",
            params![t.title, t.artist, t.album, t.genre, t.bpm, t.rating, t.time, t.key, t.date_added, mk],
        )?;

        let song_id: Option<i64> = db
            .query_row(
                "SELECT id FROM songs WHERE match_key = ?",
                params![mk],
                |row| row.get(0),
            )
            .ok();

        if let Some(sid) = song_id {
            db.execute(
                "INSERT OR IGNORE INTO playlist_songs (playlist_id, song_id) VALUES (?, ?)",
                params![playlist_id, sid],
            )?;
            added += 1;
        }
    }

    tracing::info!(
        "Imported playlist '{}': {} tracks ({} new songs)",
        playlist_name,
        track_count,
        added
    );

    Ok(serde_json::json!({
        "id": playlist_id,
        "name": playlist_name,
        "track_count": track_count,
    }))
}

#[tauri::command]
pub async fn delete_playlist(
    playlist_id: i64,
    state: State<'_, AppState>,
) -> Result<(), AppError> {
    let db = state.db.lock().unwrap();
    db.execute(
        "DELETE FROM playlist_songs WHERE playlist_id = ?",
        params![playlist_id],
    )?;
    db.execute("DELETE FROM playlists WHERE id = ?", params![playlist_id])?;
    Ok(())
}

#[tauri::command]
pub async fn get_tracks(
    playlist_id: i64,
    state: State<'_, AppState>,
) -> Result<Vec<PlaylistTrack>, AppError> {
    let db = state.db.lock().unwrap();

    let mut stmt = db.prepare(
        "SELECT s.id AS song_id, s.title, s.artist, s.album, s.genre, s.bpm, s.rating, s.time, s.key_sig, s.date_added, s.format, s.has_aiff, ps.id AS ps_id FROM playlist_songs ps JOIN songs s ON s.id = ps.song_id WHERE ps.playlist_id = ? ORDER BY ps.id",
    )?;

    let rows = stmt
        .query_map(params![playlist_id], |row| {
            Ok(PlaylistTrack {
                song_id: row.get(0)?,
                title: row.get(1)?,
                artist: row.get(2)?,
                album: row.get(3)?,
                genre: row.get(4)?,
                bpm: row.get(5)?,
                rating: row.get(6)?,
                time: row.get(7)?,
                key_sig: row.get(8)?,
                date_added: row.get(9)?,
                format: row.get(10)?,
                has_aiff: row.get(11)?,
                ps_id: row.get(12)?,
            })
        })?
        .collect::<Result<Vec<_>, _>>()?;

    Ok(rows)
}

#[tauri::command]
pub async fn export_tracks(
    playlist_id: i64,
    format: Option<String>,
    state: State<'_, AppState>,
) -> Result<serde_json::Value, AppError> {
    let db = state.db.lock().unwrap();

    let rows: Vec<(String, Option<String>)> = if let Some(ref fmt) = format {
        let mut stmt = db.prepare(
            "SELECT s.title, s.artist FROM playlist_songs ps JOIN songs s ON s.id = ps.song_id WHERE ps.playlist_id = ? AND s.format = ? ORDER BY ps.id",
        )?;
        stmt.query_map(params![playlist_id, fmt], |row| {
            Ok((row.get::<_, String>(0)?, row.get::<_, Option<String>>(1)?))
        })?
        .collect::<Result<Vec<_>, _>>()?
    } else {
        let mut stmt = db.prepare(
            "SELECT s.title, s.artist FROM playlist_songs ps JOIN songs s ON s.id = ps.song_id WHERE ps.playlist_id = ? ORDER BY ps.id",
        )?;
        stmt.query_map(params![playlist_id], |row| {
            Ok((row.get::<_, String>(0)?, row.get::<_, Option<String>>(1)?))
        })?
        .collect::<Result<Vec<_>, _>>()?
    };

    let lines: Vec<String> = rows
        .into_iter()
        .map(|(title, artist)| match artist {
            Some(ref a) if !a.is_empty() => format!("{} - {}", a, title),
            _ => title,
        })
        .collect();

    let count = lines.len();
    Ok(serde_json::json!({ "tracks": lines, "count": count }))
}
