# Eyebags Terminal

A desktop DJ utility for managing music playlists and converting audio files to AIFF. Built with **Qt 6 + C++** — single native binary, no Electron, no web runtime.

---

## Stack

| Layer | Technology |
|---|---|
| UI | Qt 6 Widgets (C++) |
| Database | SQLite via QSqlDatabase |
| Conversion | FFmpeg subprocess via QProcess |
| Folder watching | QFileSystemWatcher |
| Build | CMake 3.21+ |

---

## Dev workflow

There are three loops. They do not interfere — they share the same source in WSL and produce separate build outputs.

```
┌─────────────────────────────────────────────────────┐
│  Edit code in WSL2                                  │
│  /home/kailazy/projects/ordnung/ordnung-qt/         │
└──────────┬──────────────────┬───────────────────────┘
           │                  │
           ▼                  ▼
  ./dev.sh               dev.bat (Windows)
  Linux binary           Windows .exe
  WSLg window            Native Win32 window
  build/                 C:\projects\ordnung-build\
  (fast loop)            (test real Windows behaviour)
           │                  │
           └────────┬─────────┘
                    ▼
            git push → GitHub Actions
            builds Linux + Windows + macOS
            uploads artifacts to Actions tab
```

### Loop 1 — Daily iteration (WSLg, fastest)

WSLg runs the Linux binary as a native-looking window on your Windows desktop. One command, instant feedback.

```bash
# In WSL terminal
cd ordnung-qt
./dev.sh
```

**Requires WSLg** (Windows 11, included by default). Verify with `wsl --version` in PowerShell.

### Loop 2 — Windows native build (test real Windows behaviour)

Reads source directly from WSL — no file copying needed. Output goes to a Windows-native path so MSVC doesn't have to write across the WSL boundary.

```cmd
cd ordnung-qt
dev.bat
```

Run this when you want to verify Windows-specific behaviour: system fonts, file dialogs, DPI scaling, keyboard shortcuts. Not needed on every iteration — use Loop 1 for that.

**One-time setup:**

1. Install MSVC and CMake — paste into PowerShell (Admin):
   ```powershell
   winget install Microsoft.VisualStudio.2022.BuildTools --override "--add Microsoft.VisualStudio.Workload.VCTools --includeRecommended --passive"
   winget install Kitware.CMake
   ```

2. Install Qt6 — download the installer at [qt.io/download-qt-installer](https://qt.io/download-qt-installer) (free account), select:
   - Qt → Qt 6.x.x → **MSVC 2022 64-bit**
   - Qt → Qt 6.x.x → **Qt SVG**

3. Edit `ordnung-qt/dev.bat` — set `QT_PATH` to match your install:
   ```bat
   set QT_PATH=C:\Qt\6.7.2\msvc2022_64
   ```

**If builds feel slow** (WSL filesystem reads over `\\wsl$\` can lag), create a Windows junction point so CMake sees a local path:
```powershell
# Run once in PowerShell (Admin)
New-Item -ItemType Junction -Path C:\projects\ordnung -Target \\wsl$\Ubuntu\home\kailazy\projects\ordnung
```
Then update `dev.bat`:
```bat
set SOURCE=C:\projects\ordnung\ordnung-qt
```

### Loop 3 — Cross-platform (GitHub Actions)

Push to `master` → CI builds Linux, Windows, and macOS automatically. You do not need a Mac to produce a Mac build.

Download artifacts from the **Actions** tab in GitHub after a run completes.

---

## GitHub Actions setup

Add `.github/workflows/build.yml` to the repo:

```yaml
name: Build

on:
  push:
    branches: [master]
  pull_request:

jobs:
  build-linux:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - uses: jurplel/install-qt-action@v3
        with:
          version: '6.7.0'
          modules: 'qtsvg'
      - run: cmake -B build -DCMAKE_BUILD_TYPE=Release
      - run: cmake --build build -j$(nproc)
      - uses: actions/upload-artifact@v4
        with:
          name: Ordnung-linux
          path: build/Ordnung

  build-windows:
    runs-on: windows-latest
    steps:
      - uses: actions/checkout@v4
      - uses: jurplel/install-qt-action@v3
        with:
          version: '6.7.0'
          modules: 'qtsvg'
      - run: cmake -B build -G "Visual Studio 17 2022" -A x64
      - run: cmake --build build --config Release
      - run: windeployqt build\Release\Ordnung.exe
      - uses: actions/upload-artifact@v4
        with:
          name: Ordnung-windows
          path: build\Release\

  build-macos:
    runs-on: macos-latest
    steps:
      - uses: actions/checkout@v4
      - uses: jurplel/install-qt-action@v3
        with:
          version: '6.7.0'
          modules: 'qtsvg'
      - run: cmake -B build -DCMAKE_BUILD_TYPE=Release
      - run: cmake --build build -j$(nproc)
      - run: macdeployqt build/Ordnung.app -dmg
      - uses: actions/upload-artifact@v4
        with:
          name: Ordnung-mac
          path: build/Ordnung.dmg
```

---

## Release build (local)

```bash
# Linux
cd ordnung-qt
cmake -B build-release -DCMAKE_BUILD_TYPE=Release
cmake --build build-release -j$(nproc)
```

```cmd
:: Windows
cd ordnung-qt
cmake -B build-release -G "Visual Studio 17 2022" -A x64 -DCMAKE_PREFIX_PATH="%QT_PATH%"
cmake --build build-release --config Release
windeployqt build-release\Release\Ordnung.exe
```

---

## Data

The SQLite database is created automatically on first run:

| Platform | Path |
|---|---|
| Linux | `~/.local/share/eyebags-terminal/eyebags.db` |
| Windows | `%APPDATA%\eyebags-terminal\eyebags.db` |
| macOS | `~/Library/Application Support/eyebags-terminal/eyebags.db` |

FFmpeg must be on `PATH` at runtime for audio conversion.
