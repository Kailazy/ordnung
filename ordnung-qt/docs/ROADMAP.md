# Ordnung / Eyebags Terminal — Feature Roadmap

## Completed

- [x] **Phase 1: Foundation** — Track/CuePoint structs, DB migrations, ServiceRegistry
- [x] **Phase 2: CollectionTreePanel** — unified tree, drag-reorder, context menus
- [x] **Phase 3: Track table columns** — Color, Bitrate, Comment, click-to-cycle color labels
- [x] **Phase 4: Cue point editor** — in/out loops, hot-cues, DB CRUD, TrackDetailPanel integration
- [x] **Phase 5: Player bar** — play/pause, scrub, volume; hover play overlay on thumbnails
- [x] **Phase 6a: Rekordbox XML export** — ExportWizard 2-step dialog + ExportService XML writer
- [x] **Phase 7a: Audio analysis (ffprobe)** — AudioAnalyzer + AnalysisProgressDialog

## Upcoming

- [x] **Phase 6b: CDJ USB direct export** — PdbWriter writing Pioneer `export.pdb` binary (no Rekordbox needed)
- [x] **Phase 7b: Essentia deep analysis** — replace ffprobe with Essentia BeatTrackerMultiFeature, KeyExtractor, Discogs-Effnet model (genre/mood/danceability/vocal tags; ONNX runtime)
- [ ] **Phase 8: AI track descriptions** — Discogs-Effnet tags + small local LLM (llama.cpp/Mistral) → natural language descriptions e.g. "peak time techno with spacey pads"
- [ ] **Phase 9: Waveform renderer** — peak data computation, WaveformWidget in TrackDetailPanel
- [ ] **Phase 10: ANLZ generation** — real beat grid + waveform data written to CDJ ANLZ files (DAT/EXT) for USB export
- [x] **Phase 11: FTS5 full-text search** — title/artist/album/genre with BM25 ranking
- [ ] **Phase 12: Duplicate detector** — match by match_key or similar BPM+duration
- [x] **Phase 13: Batch metadata editor** — multi-select inline title/artist/genre/BPM editing
- [x] **Phase 14: Missing file relocator** — detect broken paths, let user relink
- [ ] **Phase 15: Preparation mode** — mark tracks as prepared, color-coded cue states
- [ ] **Phase 16: Additional exports** — Serato crates, M3U
- [ ] **Phase 17: Track history** — log played/exported tracks with timestamps
