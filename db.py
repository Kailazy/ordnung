import sqlite3
import os
from paths import get_data_dir

DB_PATH = os.path.join(get_data_dir(), "eyebags.db")


def get_connection():
    conn = sqlite3.connect(DB_PATH)
    conn.row_factory = sqlite3.Row
    conn.execute("PRAGMA journal_mode=WAL")
    conn.execute("PRAGMA foreign_keys=ON")
    return conn


def _make_match_key(artist, title):
    a = (artist or "").strip().lower()
    t = (title or "").strip().lower()
    return f"{a}|||{t}"


def init_db():
    conn = get_connection()

    conn.executescript("""
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
            status        TEXT DEFAULT 'pending' CHECK(status IN ('pending','converting','done','failed','skipped')),
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
    """)
    conn.commit()

    _migrate_tracks_to_songs(conn)
    _migrate_remove_format_constraint(conn)

    conn.close()


def _migrate_tracks_to_songs(conn):
    """One-time migration: move data from old `tracks` table into `songs` + `playlist_songs`."""
    table_check = conn.execute(
        "SELECT name FROM sqlite_master WHERE type='table' AND name='tracks'"
    ).fetchone()
    if not table_check:
        return

    print("Migrating old tracks table to songs + playlist_songs...")

    rows = conn.execute("SELECT * FROM tracks ORDER BY id").fetchall()
    if not rows:
        conn.execute("DROP TABLE tracks")
        conn.commit()
        print("Old tracks table was empty, dropped.")
        return

    migrated = 0
    linked = 0
    for r in rows:
        mk = _make_match_key(r["artist"], r["title"])

        conn.execute("""
            INSERT OR IGNORE INTO songs
                (title, artist, album, genre, bpm, rating, time, key_sig, date_added, format, has_aiff, match_key)
            VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, 0, ?)
        """, (r["title"], r["artist"], r["album"], r["genre"], r["bpm"],
              r["rating"], r["time"], r["key_sig"], r["date_added"], r["format"], mk))

        if r["format"] == "flac":
            conn.execute("UPDATE songs SET format = 'flac' WHERE match_key = ? AND format = 'mp3'", (mk,))

        song_row = conn.execute("SELECT id FROM songs WHERE match_key = ?", (mk,)).fetchone()
        if song_row:
            conn.execute("""
                INSERT OR IGNORE INTO playlist_songs (playlist_id, song_id)
                VALUES (?, ?)
            """, (r["playlist_id"], song_row["id"]))
            linked += 1

        migrated += 1

    conn.execute("DROP TABLE tracks")

    try:
        conn.execute("DROP INDEX IF EXISTS idx_tracks_playlist")
    except Exception:
        pass

    conn.commit()
    print(f"Migration complete: {migrated} track rows -> {linked} playlist_songs links")


def _migrate_remove_format_constraint(conn):
    """Remove the CHECK(format IN ('mp3','flac')) constraint from the songs table."""
    table_info = conn.execute(
        "SELECT sql FROM sqlite_master WHERE type='table' AND name='songs'"
    ).fetchone()
    if not table_info:
        return
    if "CHECK(format IN" not in table_info["sql"]:
        return

    print("Migrating songs table: removing format CHECK constraint...")

    # Disable foreign keys so DROP TABLE doesn't cascade-delete playlist_songs
    conn.execute("PRAGMA foreign_keys=OFF")
    conn.execute("""
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
        )
    """)
    conn.execute("INSERT INTO songs_new SELECT * FROM songs")
    conn.execute("DROP TABLE songs")
    conn.execute("ALTER TABLE songs_new RENAME TO songs")
    conn.execute("CREATE INDEX IF NOT EXISTS idx_songs_match_key ON songs(match_key)")
    conn.commit()
    conn.execute("PRAGMA foreign_keys=ON")

    print("Migration complete: format column now accepts any audio type.")


if __name__ == "__main__":
    init_db()
    print(f"Database initialized at {DB_PATH}")
