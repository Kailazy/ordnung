# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Agent Behavior

- When a request is ambiguous or missing details that would meaningfully affect the approach, ask a concise clarifying question before proceeding.
- Never run `cmake --build` or any build command yourself. After making code changes, tell the user to build manually (e.g. `cmake --build ordnung-qt/build --parallel`) and describe what they will see in the new build.
- Before starting any feature work, consult `ordnung-qt/docs/ROADMAP.md` for the current feature checklist. Cross off items as they complete (change `[ ]` to `[x]`).

## Project Overview

**Eyebags Terminal** (project dir: `ordnung-qt/`) is a desktop DJ library manager built with **Qt6 + C++17** and a CMake build system. It targets Windows (MinGW toolchain) with source developed in WSL.

**Build command:**
```bash
cmake --build ordnung-qt/build --parallel
```

Source lives in `ordnung-qt/src/`. There are no tests, linter configs, or formatter configs in this project.

## Architecture

### Source Layout (`ordnung-qt/src/`)

```
src/
  app/          — Application entry point and MainWindow
  core/         — Plain data structs (Track, CuePoint, ServiceRegistry)
  models/       — Qt item models (TrackModel, DownloadsModel)
  views/        — QWidget subclasses for all panels and dialogs
  delegates/    — Custom QStyledDelegate subclasses (FormatDelegate, StatusDelegate)
  services/     — Business logic (Database, Converter, FolderWatcher, PlaylistImporter, LibraryScanner)
  style/        — Theme constants (Theme.h)
```

### Style

- Visual constants (colors, spacing, font sizes) live in `src/style/Theme.h`.
- QSS stylesheet is in `resources/theme.qss` and loaded at runtime.

### Database

SQLite via `QSqlDatabase`. The database file is stored in the OS app data directory. Schema is created and migrated in `src/services/Database.cpp`.
