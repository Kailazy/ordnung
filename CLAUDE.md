# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Agent Behavior

- When a request is ambiguous or missing details that would meaningfully affect the approach, ask a concise clarifying question before proceeding.

## Project Overview

**Eyebags Terminal** is a desktop DJ utility for managing music playlists and converting audio files to AIFF format. It runs as a local Flask web server with a native window powered by PyWebView + PyQt6. FFmpeg is required at runtime for audio conversion.

## Development Commands

```bash
# Run in development mode (opens in browser, no PyWebView window)
python app.py --dev

# Run normally (opens PyWebView desktop window)
python app.py

# Build Windows executable
build.bat
# → runs: pip install -r requirements.txt && pyinstaller eyebags.spec --clean --noconfirm
# → output: dist\Eyebags Terminal\Eyebags Terminal.exe
```

There are no tests, linter configs, or formatter configs in this project.

## Architecture

### Three-Layer Stack

```
Frontend (static/app.js + style.css)
    ↓ REST API calls
Backend (app.py — Flask)
    ↓ sqlite3 queries
Database (eyebags.db — SQLite)
```

**Desktop window:** PyWebView wraps the Flask server in a native OS window. The `--dev` flag skips PyWebView and serves to browser only.

### Key Modules

- **`app.py`** — Flask app with all REST endpoints. Manages startup/shutdown lifecycle: re-queues interrupted conversions, restarts the folder watcher if previously configured. Auto-discovers an available port starting at 5000.
- **`db.py`** — SQLite setup with WAL mode, foreign keys, schema creation, and two migrations (tracks→songs/playlist_songs restructure; removed CHECK constraint on format).
- **`converter.py`** — Background `threading.Thread` with a `queue.Queue` processing conversion jobs. Calls FFmpeg as a subprocess (looks in PATH, Chocolatey, WinGet on Windows). Moves source files to `_converted/` subfolder after conversion.
- **`watcher.py`** — Watchdog-based file system observer that waits for file size stabilization before inserting new downloads. Triggers auto-conversion if enabled in config.
- **`importer.py`** — Parses Rekordbox tab-separated export files. Auto-detects encoding (UTF-8, UTF-16, UTF-8-sig).
- **`paths.py`** — Distinguishes bundled (PyInstaller `sys._MEIPASS`) vs. dev mode for resource paths.
- **`logger.py`** — Rotating file handler (5 MB × 5 files). Suppresses Werkzeug and Watchdog verbose output.

### Database Schema

```
playlists      — imported DJ playlists
songs          — track metadata (title, artist, genre, bpm, format, rating, duration, key)
playlist_songs — many-to-many join (songs shared across playlists)
downloads      — audio files detected in watch folder
conversions    — conversion job history & status
config         — app settings (watch_folder, output_folder, auto_convert)
```

Deduplication uses `match_key = "artist|||title"` (normalized, lowercase).

### Frontend State (`static/app.js`)

Single-page app with vanilla JS. State object:
```javascript
state = {
  tab, playlists, activePlaylist, tracks, search,
  genreFilter, expandedSong, songPlaylistsCache,
  downloads, watchConfig
}
```

Two tabs: **Library** (playlist/track management, bulk format updates, genre filtering) and **Downloads** (watch folder config, conversion triggering, conversion history).

### Configuration

All settings stored in the SQLite `config` table—no config files. Runtime artifacts (`eyebags.db`, `logs/eyebags.log`) are placed next to the executable in production or in the project directory in dev mode.
