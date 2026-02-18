use rusqlite::params;
use tauri::State;

use crate::error::AppError;
use crate::models::SongPlaylist;
use crate::AppState;

#[tauri::command]
pub async fn update_song_format(
    song_id: i64,
    format: String,
    state: State<'_, AppState>,
) -> Result<(), AppError> {
    let fmt = format.trim().to_lowercase();
    if fmt.is_empty() {
        return Err(AppError::Other("Format cannot be empty".to_string()));
    }
    let db = state.db.lock().unwrap();
    db.execute("UPDATE songs SET format = ? WHERE id = ?", params![fmt, song_id])?;
    Ok(())
}

#[tauri::command]
pub async fn update_song_aiff(
    song_id: i64,
    has_aiff: bool,
    state: State<'_, AppState>,
) -> Result<(), AppError> {
    let val: i64 = if has_aiff { 1 } else { 0 };
    let db = state.db.lock().unwrap();
    db.execute("UPDATE songs SET has_aiff = ? WHERE id = ?", params![val, song_id])?;
    Ok(())
}

#[tauri::command]
pub async fn bulk_update_format(
    format: String,
    ids: Option<Vec<i64>>,
    playlist_id: Option<i64>,
    state: State<'_, AppState>,
) -> Result<(), AppError> {
    let fmt = format.trim().to_lowercase();
    if fmt.is_empty() {
        return Err(AppError::Other("Format cannot be empty".to_string()));
    }

    let db = state.db.lock().unwrap();

    if let Some(ref id_list) = ids {
        if !id_list.is_empty() {
            // Build a parameterized query with placeholders
            let placeholders: Vec<String> = id_list.iter().map(|_| "?".to_string()).collect();
            let sql = format!(
                "UPDATE songs SET format = ? WHERE id IN ({})",
                placeholders.join(",")
            );
            let mut stmt = db.prepare(&sql)?;
            let mut param_values: Vec<Box<dyn rusqlite::types::ToSql>> = Vec::new();
            param_values.push(Box::new(fmt.clone()));
            for id in id_list {
                param_values.push(Box::new(*id));
            }
            let params_ref: Vec<&dyn rusqlite::types::ToSql> =
                param_values.iter().map(|p| p.as_ref()).collect();
            stmt.execute(params_ref.as_slice())?;
        }
    } else if let Some(pid) = playlist_id {
        db.execute(
            "UPDATE songs SET format = ? WHERE id IN (SELECT song_id FROM playlist_songs WHERE playlist_id = ?)",
            params![fmt, pid],
        )?;
    } else {
        db.execute("UPDATE songs SET format = ?", params![fmt])?;
    }

    Ok(())
}

#[tauri::command]
pub async fn get_song_playlists(
    song_id: i64,
    state: State<'_, AppState>,
) -> Result<Vec<SongPlaylist>, AppError> {
    let db = state.db.lock().unwrap();

    let mut all_stmt = db.prepare("SELECT id, name FROM playlists ORDER BY name")?;
    let all_playlists: Vec<(i64, String)> = all_stmt
        .query_map([], |row| Ok((row.get::<_, i64>(0)?, row.get::<_, String>(1)?)))?
        .collect::<Result<Vec<_>, _>>()?;

    let mut member_stmt =
        db.prepare("SELECT playlist_id FROM playlist_songs WHERE song_id = ?")?;
    let member_ids: std::collections::HashSet<i64> = member_stmt
        .query_map(params![song_id], |row| row.get::<_, i64>(0))?
        .collect::<Result<std::collections::HashSet<_>, _>>()?;

    let result: Vec<SongPlaylist> = all_playlists
        .into_iter()
        .map(|(id, name)| SongPlaylist {
            id,
            name,
            member: member_ids.contains(&id),
        })
        .collect();

    Ok(result)
}

#[tauri::command]
pub async fn update_song_playlists(
    song_id: i64,
    add: Vec<i64>,
    remove: Vec<i64>,
    state: State<'_, AppState>,
) -> Result<(), AppError> {
    let db = state.db.lock().unwrap();

    for pid in &add {
        db.execute(
            "INSERT OR IGNORE INTO playlist_songs (playlist_id, song_id) VALUES (?, ?)",
            params![pid, song_id],
        )?;
    }

    for pid in &remove {
        db.execute(
            "DELETE FROM playlist_songs WHERE playlist_id = ? AND song_id = ?",
            params![pid, song_id],
        )?;
    }

    Ok(())
}
