# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Agent Behavior

- When a request is ambiguous or missing details that would meaningfully affect the approach, ask a concise clarifying question before proceeding.

## Project Overview

**Eyebags Terminal** is a desktop DJ utility for managing music playlists and converting audio files to AIFF format. It is built with **Tauri 2 + Svelte 5** (Rust backend, reactive frontend). FFmpeg is required at runtime for audio conversion.

## Development Commands

```bash
# Run full app (Tauri window + Vite dev server — requires Rust/Cargo)
npm run tauri dev

# Run frontend only in browser (no Tauri, no Rust required)
npm run dev
# → Vite dev server at http://localhost:1420

# Build production app bundle
npm run tauri build
# → Output in src-tauri/target/release/bundle/
```

There are no tests, linter configs, or formatter configs in this project.

## Architecture

### Two-Layer Stack

```
Frontend (src/ — Svelte 5)
    ↓ Tauri invoke() IPC calls
Backend (src-tauri/src/ — Rust)
    ↓ rusqlite queries
Database (eyebags.db — SQLite, stored in OS app data dir)
```

**Desktop window:** Tauri wraps a Vite-built Svelte SPA in a native OS window. `npm run dev` serves the frontend alone in-browser for UI iteration without Rust.

### Frontend (`src/`)

- **`main.js`** — Entry point, mounts the Svelte app.
- **`App.svelte`** — Root component; tab switcher between Library and Downloads.
- **`app.css`** — Global styles.
- **`lib/stores.svelte.js`** — Single Svelte 5 `$state` reactive store shared across all components.
- **`lib/invoke.js`** — Thin wrappers around `@tauri-apps/api/core` `invoke()` for every Tauri command.
- **`lib/Library.svelte`** — Library tab (playlists, tracks, genre filter, bulk format updates).
- **`lib/Downloads.svelte`** — Downloads tab (watch folder config, conversion triggering, activity log).
- **`lib/components/`** — Granular UI components: `PlaylistSidebar`, `TrackList`, `TrackRow`, `TrackDetail`, `GenreBar`, `FolderFlow`, `DownloadList`, `ActivityLog`.

#### Frontend State (`lib/stores.svelte.js`)

```javascript
store = {
  tab, playlists, activePlaylist, tracks, search,
  genreFilter, formatUndo, expandedSong, songPlaylistsCache,
  downloads, watchConfig, modalContent, logLines,
}
```

### Backend (`src-tauri/src/`)

- **`lib.rs`** — Tauri app setup: registers plugins (dialog, shell, fs), initialises `AppState`, calls `restore_on_startup`, registers all invoke handlers.
- **`db.rs`** — Opens SQLite with WAL + foreign keys; runs schema creation and two migrations (tracks→songs/playlist_songs restructure; removed CHECK constraint on format). DB stored via `app.path().app_data_dir()`.
- **`converter.rs`** — `ConversionWorker`: a Tokio-based background worker with a channel queue. Calls FFmpeg as a subprocess. Emits Tauri events on progress/completion.
- **`watcher.rs`** — `FolderWatcher`: uses `notify`/`notify-debouncer-mini` to watch a folder for new audio files; triggers auto-conversion if configured.
- **`importer.rs`** — Parses Rekordbox tab-separated export files. Auto-detects encoding (`encoding_rs`: UTF-8, UTF-16, UTF-8-sig).
- **`models.rs`** — Serde-serialisable structs returned by commands.
- **`error.rs`** — `AppError` enum with `thiserror` for unified error handling.
- **`commands/`** — Tauri `#[tauri::command]` handlers, split by domain:
  - `playlists.rs` — `list_playlists`, `import_playlist`, `delete_playlist`, `get_tracks`, `export_tracks`
  - `songs.rs` — `update_song_format`, `update_song_aiff`, `bulk_update_format`, `get_song_playlists`, `update_song_playlists`
  - `downloads.rs` — `list_downloads`, `delete_download`, `scan_folder`, `browse_folder`
  - `conversions.rs` — `list_conversions`, `conversion_stats`, `retry_conversion`, `convert_single`
  - `config.rs` — `get_watch_config`, `set_watch_config`

#### Shared App State (`AppState`)

```rust
pub struct AppState {
    pub db: Arc<Mutex<Connection>>,
    pub converter: Arc<ConversionWorker>,
    pub watcher: Arc<Mutex<FolderWatcher>>,
}
```

### Database Schema

```
playlists      — imported DJ playlists
songs          — track metadata (title, artist, album, genre, bpm, rating, time, key_sig, date_added, format, has_aiff, match_key)
playlist_songs — many-to-many join (songs shared across playlists)
downloads      — audio files detected in watch folder
conversions    — conversion job history & status
config         — app settings (watch_folder, output_folder, auto_convert)
```

Deduplication uses `match_key = "artist|||title"` (normalised, lowercase).

### Key Dependencies

| Crate | Purpose |
|---|---|
| `tauri 2` | Desktop app shell & IPC |
| `rusqlite` (bundled) | SQLite database |
| `tokio` (full) | Async runtime for converter |
| `notify` + `notify-debouncer-mini` | Folder watching |
| `serde` + `serde_json` | Serialisation |
| `tracing` + `tracing-subscriber` | Structured logging |
| `encoding_rs` | Playlist file encoding detection |
| `thiserror` | Error types |
| `chrono` | Timestamps |

### Configuration

All settings stored in the SQLite `config` table — no config files. The database is placed in the OS app data directory (`app.path().app_data_dir()`), which is typically:
- **Linux:** `~/.local/share/com.eyebags.terminal/`
- **Windows:** `%APPDATA%\com.eyebags.terminal\`
- **macOS:** `~/Library/Application Support/com.eyebags.terminal/`
