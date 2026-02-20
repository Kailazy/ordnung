# Sound file manager — architecture notes

## Product focus

The app is a **sound file manager**: it discovers audio files (in a chosen library folder), tracks each file’s **format** (and metadata), and lets you change or normalize formats. Automatic conversion to a target format (e.g. AIFF) is supported in code and can be re‑enabled when the Downloads workflow is revisited.

## Data and flows

### Library (primary surface)

- **Source of truth:** User picks a **library folder** (popup “library” → “set folder”). That path is stored in `config.library_folder`.
- **Scan:** `LibraryScanner::scan(folder)` walks the tree, finds audio files by extension, reads metadata (ffprobe: duration, BPM, key), and builds `Track` structs with `format` = file extension (mp3, flac, wav, aiff, etc.).
- **Persistence:** Tracks are upserted into **songs** (and optionally **playlist_songs**). Each song has `format`, `filepath`, and metadata. So we **track file types** (and metadata) for every file we find.
- **UI:** Library view shows the track table (format column, bulk “apply to all”, undo). Edits to title/artist/BPM/key/time/format are persisted via `Database::updateSongMetadata` / format commands.

### File type tracking

- **Format** is stored per song in `songs.format` and shown in the library table. It reflects the on‑disk extension at scan time and can be edited (e.g. after converting the file elsewhere).
- **Bulk format** updates only change the stored `format` in the DB; they do not run conversion. Conversion is a separate path (Converter + conversions table).

### Conversion (kept in code, no tab for now)

- **ConversionWorker** (in a background thread) runs FFmpeg to convert a source file to AIFF (or other target) and records the job in **conversions** (source_path, output_path, status). It is driven by `enqueue(downloadId, sourcePath, outputFolder)`.
- **FolderWatcher** can watch a folder, detect new audio files, and insert them into **downloads**. It does not auto‑convert by itself; no connection from `fileDetected` to the converter exists in the current UI. Watch config (watch_folder, output_folder, auto_convert) is stored but the **Downloads tab** has been removed; conversion and watch logic remain in code for when that workflow is revisited.
- **downloads** and **conversions** tables stay in the DB so that re‑adding a “downloads” or “convert” UI later does not require schema changes.

### What was removed from the UI

- The **Downloads** tab (sidebar + stack page) and all MainWindow wiring that drove it: scan button, convert all/single, delete download, conversion status updates, activity log. The **sound file manager** surface is the Library only; conversion and watch services are still built and run but have no UI until you re‑add a tab or actions (e.g. “Convert selection to AIFF” from the library).

## Summary

- **Sound file manager:** Library folder → scan → tracks with **file type (format)** and metadata → edit/bulk format in UI; DB stores everything.
- **Conversion:** Converter + FolderWatcher + downloads/conversions DB stay; UI for them (Downloads tab) removed and can be revisited later.

---

## Development phases (Qt rewrite roadmap)

### Phase 1 — Foundation ✓
Extend `Track` struct (color label, bitrate, comment, cue points), DB migrations, `ServiceRegistry`, `CuePoint` struct.

### Phase 2 — CollectionTreePanel
Replace `PlaylistPanel` + `FolderPanel` with a unified `CollectionTreePanel`: tree view with Playlists / Folders / All Tracks nodes, drag-reorder, context-menu actions.

### Phase 3 — Track table columns
Add Color, Bitrate, and Comment columns to the track table. Color label is click-to-cycle through the standard DJ color palette.

### Phase 4 — Cue point editor
`CuePointEditor` widget with in/out loop markers, hot-cue labels, DB CRUD for `cue_points` table, integrated into `TrackDetailPanel`.

### Phase 5 — Audio preview / player bar
Basic waveform-less playback bar (Qt Multimedia): play/pause, scrub, volume. No full waveform rendering yet.

### Phase 6 — Rekordbox-compatible export
`ExportService` + `ExportWizard` dialog. Two export targets:

1. **Rekordbox XML** — writes `rekordbox.xml` (playlists + track metadata + cue points) importable directly into Rekordbox software.
2. **USB export (CDJ-ready)** — formats and writes the Rekordbox database layout directly to a USB drive so CDJs can read it without opening Rekordbox:
   - PIONEER folder structure (`PIONEER/rekordbox/export.pdb` or equivalent `.edb`).
   - Copies audio files into the expected directory layout.
   - Writes analysis stubs (`.DAT`/`.EXT` ANLZ files) so the CDJ shows waveform/BPM data if pre-analysed.
   - ExportWizard lets the user pick: target USB mount point, which playlists to include, output format (keep original / convert to AIFF/WAV).

### Phase 7 — Audio analysis integration
BPM/key re-analysis via `aubio` or `essentia` CLI; waveform data generation for ANLZ export in Phase 6.
