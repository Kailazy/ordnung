# Eyebags Terminal

A desktop DJ library manager for managing tracks, playlists, cue points, and audio analysis. Built with **Qt 6 + C++17** — single native binary, no Electron, no web runtime.

---

## Stack

| Layer | Technology |
|---|---|
| UI | Qt 6 Widgets (C++) |
| Database | SQLite via `QSqlDatabase` |
| Audio conversion | FFmpeg subprocess via `QProcess` |
| Library scanning | `LibraryScanner` (recursive, watcher-based) |
| Audio analysis | Essentia (BPM, key, mood/style) + ffprobe/aubio fallback |
| Genre/mood tags | Discogs-Effnet ONNX model (optional) |
| Folder watching | `QFileSystemWatcher` |
| Build | CMake 3.21+ |

---

## Building

```bash
# From repo root (WSL or Linux)
cmake -B ordnung-qt/build -S ordnung-qt -DCMAKE_BUILD_TYPE=Release
cmake --build ordnung-qt/build --parallel
```

Qt 6.7+ must be on `CMAKE_PREFIX_PATH`. The build is optional-dependency-aware: Essentia and ONNX Runtime are used if found in `third_party/`, otherwise the app compiles and runs with the ffprobe/aubio analysis fallback.

---

## Dev workflow

Three loops. They share the same source and don't interfere.

```
┌─────────────────────────────────────────────────────┐
│  Edit code in WSL2                                  │
│  /home/kailazy/projects/ordnung/ordnung-qt/         │
└──────────┬──────────────────┬───────────────────────┘
           │                  │
           ▼                  ▼
  cmake --build          dev.bat (Windows)
  Linux binary           Windows .exe
  WSLg window            Native Win32 window
  (fast loop)            (test real Windows behaviour)
           │                  │
           └────────┬─────────┘
                    ▼
            git push → GitHub Actions
            builds Linux + Windows + macOS
            uploads artifacts to Actions tab
```

### Loop 1 — Daily iteration (WSLg, fastest)

WSLg runs the Linux binary as a native-looking window on your Windows desktop.

```bash
cmake --build ordnung-qt/build --parallel && ./ordnung-qt/build/Ordnung
```

Requires WSLg (Windows 11, included by default).

### Loop 2 — Windows native build

Reads source from `\\wsl$\`. Output goes to `C:\ordnung-build` to avoid writing across the WSL boundary.

**One-time setup:** Install Qt 6 via [qt.io/download-qt-installer](https://qt.io/download-qt-installer). Select:
- Qt → Qt 6.x.x → **MinGW 64-bit**
- Qt → Qt 6.x.x → **Qt SVG**, **Qt Multimedia**
- Developer and Designer Tools → **MinGW 13.x.x 64-bit**

Then build from an MSYS2 MinGW64 shell:
```bash
cmake -B build -S ordnung-qt -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

### Loop 3 — Cross-platform CI

Push to `master` → GitHub Actions builds Linux, Windows (MSYS2/MinGW64), and macOS automatically. Download artifacts from the **Actions** tab.

---

## Audio Analysis

Eyebags Terminal supports two analysis modes:

### Basic (always available)
- BPM detection via **aubio** (if installed) or ffprobe metadata
- Key from embedded file tags

### Deep analysis (optional — requires Essentia)
- Accurate BPM via `BeatTrackerMultiFeature`
- Musical key via `KeyExtractor`
- Mood, style, danceability, vocal probability via **Discogs-Effnet** ONNX model

### Enabling deep analysis

Deep analysis requires pre-built Essentia binaries in `third_party/`. These are not committed to the repo by default (they're large). Build them on the target platform:

**Linux (from WSL):**
```bash
bash scripts/build-essentia-linux.sh
```

**macOS:**
```bash
bash scripts/build-essentia-macos.sh
```

After building, commit `third_party/essentia/<platform>/` to Git LFS:
```bash
git add third_party/essentia/
git commit -m "chore: add bundled Essentia <platform>"
```

**Windows:** Essentia's waf build system does not support MinGW. Windows ships with the ffprobe/aubio fallback — BPM detection still works.

### Discogs-Effnet genre/mood model (optional)

Download the model file from [Essentia models](https://essentia.upf.edu/models/) and place it at:

| Platform | Path |
|---|---|
| Linux | `~/.local/share/eyebags-terminal/models/discogs-effnet-bs64-1.pb.onnx` |
| Windows | `%APPDATA%\eyebags-terminal\models\discogs-effnet-bs64-1.pb.onnx` |
| macOS | `~/Library/Application Support/eyebags-terminal/models/discogs-effnet-bs64-1.pb.onnx` |

Run `bash scripts/fetch-onnxruntime.sh` to download the ONNX Runtime runtime alongside Essentia.

---

## Data

The SQLite database is created automatically on first run:

| Platform | Path |
|---|---|
| Linux | `~/.local/share/eyebags-terminal/eyebags.db` |
| Windows | `%APPDATA%\eyebags-terminal\eyebags.db` |
| macOS | `~/Library/Application Support/eyebags-terminal/eyebags.db` |

FFmpeg must be on `PATH` at runtime for audio conversion.

---

## Git LFS

Binary assets in `third_party/` (Essentia and ONNX Runtime `.so`/`.dylib`/`.dll`/`.a` files) are tracked with Git LFS. Install LFS before cloning if you need them:

```bash
git lfs install
git clone --recurse-submodules <repo-url>
```
