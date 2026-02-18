use rusqlite::{Connection, Result, params};
use std::path::Path;
use crate::error::AppError;

/// Open the SQLite database, enable WAL + foreign keys, run schema and migrations.
pub fn open_db(data_dir: &Path) -> Result<Connection, AppError> {
    let path = data_dir.join("eyebags.db");
    let conn = Connection::open(&path)?;
    conn.execute_batch("PRAGMA journal_mode=WAL; PRAGMA foreign_keys=ON;")?;
    run_schema(&conn)?;
    migrate_tracks_to_songs(&conn)?;
    migrate_remove_format_constraint(&conn)?;
    Ok(conn)
}

fn run_schema(conn: &Connection) -> Result<(), AppError> {
    conn.execute_batch(
        r#"
        CREATE TABLE IF NOT EXISTS playlists (
            id          INTEGER PRIMARY KEY AUTOINCREMENT,
            name        TEXT NOT NULL,
            imported_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
        );
        CREATE TABLE IF NOT EXISTS songs (
            id          INTEGER PRIMARY KEY AUTOINCREMENT,
            title       TEXT NOT NULL DEFAULT '',
            artist      TEXT DEFAULT '',
            album       TEXT DEFAULT '',
            genre       TEXT DEFAULT '',
            bpm         TEXT DEFAULT '',
            rating      TEXT DEFAULT '',
            time        TEXT DEFAULT '',
            key_sig     TEXT DEFAULT '',
            date_added  TEXT DEFAULT '',
            format      TEXT DEFAULT 'mp3',
            has_aiff    INTEGER DEFAULT 0,
            match_key   TEXT UNIQUE
        );
        CREATE TABLE IF NOT EXISTS playlist_songs (
            id          INTEGER PRIMARY KEY AUTOINCREMENT,
            playlist_id INTEGER NOT NULL REFERENCES playlists(id) ON DELETE CASCADE,
            song_id     INTEGER NOT NULL REFERENCES songs(id) ON DELETE CASCADE,
            added_at    TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            UNIQUE(playlist_id, song_id)
        );
        CREATE TABLE IF NOT EXISTS downloads (
            id          INTEGER PRIMARY KEY AUTOINCREMENT,
            filename    TEXT NOT NULL,
            filepath    TEXT NOT NULL,
            extension   TEXT DEFAULT '',
            size_mb     REAL DEFAULT 0,
            detected_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
        );
        CREATE TABLE IF NOT EXISTS config (
            key   TEXT PRIMARY KEY,
            value TEXT
        );
        CREATE TABLE IF NOT EXISTS conversions (
            id            INTEGER PRIMARY KEY AUTOINCREMENT,
            download_id   INTEGER REFERENCES downloads(id) ON DELETE SET NULL,
            source_path   TEXT NOT NULL,
            output_path   TEXT DEFAULT '',
            source_ext    TEXT DEFAULT '',
            status        TEXT DEFAULT 'pending',
            error_msg     TEXT DEFAULT '',
            size_mb       REAL DEFAULT 0,
            started_at    TIMESTAMP,
            finished_at   TIMESTAMP,
            created_at    TIMESTAMP DEFAULT CURRENT_TIMESTAMP
        );
        CREATE INDEX IF NOT EXISTS idx_playlist_songs_playlist ON playlist_songs(playlist_id);
        CREATE INDEX IF NOT EXISTS idx_playlist_songs_song ON playlist_songs(song_id);
        CREATE INDEX IF NOT EXISTS idx_songs_match_key ON songs(match_key);
        CREATE INDEX IF NOT EXISTS idx_downloads_detected ON downloads(detected_at);
        CREATE INDEX IF NOT EXISTS idx_conversions_status ON conversions(status);
    "#,
    )?;
    Ok(())
}

/// One-time migration: move data from old `tracks` table into `songs` + `playlist_songs`.
fn migrate_tracks_to_songs(conn: &Connection) -> Result<(), AppError> {
    let exists: bool = conn.query_row(
        "SELECT COUNT(*) FROM sqlite_master WHERE type='table' AND name='tracks'",
        [],
        |row| row.get::<_, i64>(0),
    )? > 0;
    if !exists {
        return Ok(());
    }

    tracing::info!("Migrating old tracks table to songs + playlist_songs...");

    // Check if the tracks table has any rows
    let count: i64 = conn.query_row("SELECT COUNT(*) FROM tracks", [], |row| row.get(0))?;
    if count == 0 {
        conn.execute("DROP TABLE tracks", [])?;
        tracing::info!("Old tracks table was empty, dropped.");
        return Ok(());
    }

    let mut stmt = conn.prepare(
        "SELECT id, playlist_id, title, artist, album, genre, bpm, rating, time, key_sig, date_added, format FROM tracks ORDER BY id",
    )?;

    let mut migrated: i64 = 0;
    let mut linked: i64 = 0;

    let rows = stmt.query_map([], |row| {
        Ok((
            row.get::<_, i64>(0)?,          // id
            row.get::<_, i64>(1)?,          // playlist_id
            row.get::<_, String>(2)?,       // title
            row.get::<_, String>(3)?,       // artist
            row.get::<_, String>(4)?,       // album
            row.get::<_, String>(5)?,       // genre
            row.get::<_, String>(6)?,       // bpm
            row.get::<_, String>(7)?,       // rating
            row.get::<_, String>(8)?,       // time
            row.get::<_, String>(9)?,       // key_sig
            row.get::<_, String>(10)?,      // date_added
            row.get::<_, String>(11)?,      // format
        ))
    })?;

    let collected: Vec<_> = rows.collect::<Result<Vec<_>, _>>()?;

    for (_, playlist_id, title, artist, album, genre, bpm, rating, time, key_sig, date_added, format) in &collected {
        let mk = make_match_key(artist, title);

        conn.execute(
            "INSERT OR IGNORE INTO songs (title, artist, album, genre, bpm, rating, time, key_sig, date_added, format, has_aiff, match_key) VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10, 0, ?11)",
            params![title, artist, album, genre, bpm, rating, time, key_sig, date_added, format, mk],
        )?;

        if format == "flac" {
            conn.execute(
                "UPDATE songs SET format = 'flac' WHERE match_key = ? AND format = 'mp3'",
                params![mk],
            )?;
        }

        let song_id: Option<i64> = conn
            .query_row("SELECT id FROM songs WHERE match_key = ?", params![mk], |row| {
                row.get(0)
            })
            .ok();

        if let Some(sid) = song_id {
            conn.execute(
                "INSERT OR IGNORE INTO playlist_songs (playlist_id, song_id) VALUES (?, ?)",
                params![playlist_id, sid],
            )?;
            linked += 1;
        }

        migrated += 1;
    }

    conn.execute("DROP TABLE tracks", [])?;

    // Best-effort drop old index
    let _ = conn.execute("DROP INDEX IF EXISTS idx_tracks_playlist", []);

    tracing::info!(
        "Migration complete: {} track rows -> {} playlist_songs links",
        migrated,
        linked
    );
    Ok(())
}

/// Remove the CHECK(format IN ('mp3','flac')) constraint from the songs table.
fn migrate_remove_format_constraint(conn: &Connection) -> Result<(), AppError> {
    let sql: Option<String> = conn
        .query_row(
            "SELECT sql FROM sqlite_master WHERE type='table' AND name='songs'",
            [],
            |row| row.get(0),
        )
        .ok();

    let sql = match sql {
        Some(s) => s,
        None => return Ok(()),
    };

    if !sql.contains("CHECK(format IN") {
        return Ok(());
    }

    tracing::info!("Migrating songs table: removing format CHECK constraint...");

    // Disable foreign keys so DROP TABLE doesn't cascade-delete playlist_songs
    conn.execute_batch("PRAGMA foreign_keys=OFF")?;
    conn.execute_batch(
        r#"
        CREATE TABLE IF NOT EXISTS songs_new (
            id          INTEGER PRIMARY KEY AUTOINCREMENT,
            title       TEXT NOT NULL DEFAULT '',
            artist      TEXT DEFAULT '',
            album       TEXT DEFAULT '',
            genre       TEXT DEFAULT '',
            bpm         TEXT DEFAULT '',
            rating      TEXT DEFAULT '',
            time        TEXT DEFAULT '',
            key_sig     TEXT DEFAULT '',
            date_added  TEXT DEFAULT '',
            format      TEXT DEFAULT 'mp3',
            has_aiff    INTEGER DEFAULT 0,
            match_key   TEXT UNIQUE
        );
        INSERT INTO songs_new SELECT * FROM songs;
        DROP TABLE songs;
        ALTER TABLE songs_new RENAME TO songs;
        CREATE INDEX IF NOT EXISTS idx_songs_match_key ON songs(match_key);
    "#,
    )?;
    conn.execute_batch("PRAGMA foreign_keys=ON")?;

    tracing::info!("Migration complete: format column now accepts any audio type.");
    Ok(())
}

/// Build a deduplication key from artist and title.
pub fn make_match_key(artist: &str, title: &str) -> String {
    let a = artist.trim().to_lowercase();
    let t = title.trim().to_lowercase();
    format!("{}|||{}", a, t)
}
