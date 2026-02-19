# Qt Migration Plan — Ordnung

## Recommendation: Start Fresh

There is no code to "refactor" — every Svelte component, every CSS rule, every reactive binding must be rewritten as Qt C++ widgets. The backend can also be fully replaced: Qt provides `QSqlDatabase` (SQLite), `QProcess` (FFmpeg), and `QFileSystemWatcher` — everything the Rust backend does, natively. A single Qt/C++ binary is cleaner than a Qt frontend communicating over IPC with a Rust process.

**The only things that port directly:**
- Database schema (identical — same tables, same queries)
- Application logic and business rules
- UI structure and feature set (two tabs, same interactions)

---

## 1. Qt Widgets vs Qt Quick/QML

**Use Qt Widgets.**

| Concern | Qt Widgets | Qt Quick/QML |
|---|---|---|
| Page-wide text selection | Impossible — per-control, OS-native | Requires explicit work to prevent |
| "Drag HTML elements" feel | No HTML model at all | Possible if not careful with MouseArea |
| Browser scroll physics | Standard desktop scroll — no inertia | Has it by default, must be disabled |
| DOM-style interaction | No DOM, no document model | Shares conceptual DNA with HTML/JS |
| Native focus/keyboard nav | Built-in — tab order, accelerators, mnemonics | Requires manual FocusScope management |
| Native file dialogs | QFileDialog — identical to Explorer on Windows | Same API, same result |

Qt Quick is designed for touch-first UIs with fluid animation. It can be tamed, but you'd be fighting its defaults. Qt Widgets is the toolkit used by Qt Creator itself, Wireshark, VirtualBox, and most professional tools. It renders via QPainter over native platform APIs (GDI+/Direct2D on Windows).

**Exception:** for custom waveform display or OpenGL visualization, embed a `QOpenGLWidget` inside the Widgets app. Native controls everywhere, GPU-accelerated painting where needed.

---

## 2. Architecture

Use **MVP with a Command layer** — cleaner than full MVVM in C++ because Qt's signal/slot system already handles binding, and pure MVVM forces awkward two-way binding machinery.

```
┌─────────────────────────────────────────────────┐
│  View Layer (QWidget subclasses)                │
│  TrackTableView, PlaylistPanel, DownloadsView   │
│         ↑ signals / slots ↓                     │
├─────────────────────────────────────────────────┤
│  Model Layer (QAbstractItemModel subclasses)    │
│  TrackModel, PlaylistModel, DownloadsModel      │
│         ↑ signals / slots ↓                     │
├─────────────────────────────────────────────────┤
│  Command Layer (QUndoCommand subclasses)        │
│  UpdateFormatCmd, DeleteTrackCmd, ImportCmd     │
│         ↑ operates on ↓                         │
├─────────────────────────────────────────────────┤
│  Core / Domain (pure C++, zero Qt dependency)  │
│  Track, Playlist, ConversionJob structs         │
│         ↑ persisted by ↓                        │
├─────────────────────────────────────────────────┤
│  Services (QThread workers)                     │
│  Database, Converter, FolderWatcher             │
└─────────────────────────────────────────────────┘
```

**Key principle:** the Core layer has zero Qt headers. It's just structs and plain C++. This makes it trivially testable with Catch2 or GoogleTest without spinning up a QApplication.

---

## 3. Feature → Qt API Mapping

### Current Svelte/Tauri → Qt equivalent

| Current | Qt Replacement |
|---|---|
| `invoke('list_playlists')` | `Database::loadPlaylists()` → `PlaylistModel` |
| `invoke('get_tracks', playlistId)` | `Database::loadTracks(playlistId)` → `TrackModel` |
| `invoke('import_playlist', filePath)` | `PlaylistImporter::parse()` + `Database::insertPlaylist()` |
| `invoke('update_song_format')` | `UpdateFormatCommand` → `QUndoStack::push()` |
| `invoke('bulk_update_format')` | `BulkFormatCommand` → `QUndoStack::push()` |
| `invoke('browse_folder')` | `QFileDialog::getExistingDirectory()` |
| `invoke('scan_folder')` | `FolderWatcher::scan()` |
| `invoke('convert_single')` | `ConversionWorker::enqueue()` |
| `invoke('list_downloads')` | `Database::loadDownloads()` → `DownloadsModel` |
| Tauri `listen('conversion-update')` | `ConversionWorker` signal → Qt slot (queued connection) |
| Tauri `listen('log-line')` | Logger signal → `ActivityLog` slot |
| `store.search` filter | `QSortFilterProxyModel::setFilterFixedString()` |
| `store.genreFilter` | Custom `GenreProxyModel` or filter on `QSortFilterProxyModel` |
| `store.formatUndo` snapshot | `QUndoStack` with `BulkFormatCommand` |
| Svelte `$state` reactive store | `QAbstractItemModel` + `dataChanged()` signals |

### Database schema (unchanged)

```sql
playlists      — id, name, imported_at
songs          — id, title, artist, album, genre, bpm, rating, time, key_sig,
                 date_added, format, has_aiff, match_key
playlist_songs — playlist_id, song_id (many-to-many)
downloads      — id, filename, filepath, extension, size_mb, detected_at
conversions    — id, download_id, source_path, output_path, source_ext,
                 status, error_msg, size_mb, started_at, finished_at
config         — key, value
```

Use `QSqlDatabase::addDatabase("QSQLITE")` — the schema runs identically, including both migrations.

---

## 4. Project Layout

```
ordnung-qt/
├── CMakeLists.txt
├── src/
│   ├── main.cpp
│   ├── core/                      # Pure C++ — zero Qt headers
│   │   ├── Track.h
│   │   ├── Playlist.h
│   │   └── ConversionJob.h
│   ├── app/
│   │   ├── Application.h/cpp      # QApplication subclass, global init
│   │   └── MainWindow.h/cpp       # QMainWindow, menu bar, nav, undo stack
│   ├── models/                    # QAbstractItemModel subclasses
│   │   ├── TrackModel.h/cpp
│   │   ├── PlaylistModel.h/cpp
│   │   └── DownloadsModel.h/cpp
│   ├── views/                     # QWidget subclasses — layout only, no logic
│   │   ├── LibraryView.h/cpp      # Splitter: playlist panel + track area
│   │   ├── PlaylistPanel.h/cpp
│   │   ├── TrackTableView.h/cpp   # Configured QTableView + detail panel
│   │   ├── TrackDetailPanel.h/cpp # Expandable metadata + playlist membership
│   │   └── DownloadsView.h/cpp    # Folder config + table + activity log
│   ├── delegates/
│   │   ├── FormatDelegate.h/cpp   # Draws format badge (AIFF/MP3/FLAC/etc.)
│   │   └── StatusDelegate.h/cpp   # Conversion status badge
│   ├── commands/                  # QUndoCommand — all mutations go here
│   │   ├── UpdateFormatCommand.h/cpp
│   │   ├── BulkFormatCommand.h/cpp
│   │   └── DeleteTracksCommand.h/cpp
│   ├── services/
│   │   ├── Database.h/cpp         # QSqlDatabase, all queries
│   │   ├── Converter.h/cpp        # QProcess FFmpeg worker on QThread
│   │   ├── FolderWatcher.h/cpp    # QFileSystemWatcher
│   │   └── PlaylistImporter.h/cpp # Rekordbox TSV parser (encoding_rs → QTextCodec)
│   └── utils/
│       └── PathUtils.h/cpp
├── resources/
│   ├── theme.qss                  # Dark stylesheet
│   ├── icons/                     # SVG + PNG at 16/32px
│   └── resources.qrc
└── tests/
    ├── test_track_model.cpp
    └── test_importer.cpp
```

---

## 5. Implementation Strategies

### Selection and text editing

```cpp
// Display-only — never selectable (no web-page text drag)
QLabel* label = new QLabel("Artist Name");
label->setTextInteractionFlags(Qt::NoTextInteraction);

// Editable field — selection is per-control only
QLineEdit* search = new QLineEdit();
// Ctrl+A selects only within this widget. Never page-wide.

// Table cells — use delegates, never embed widgets in every cell
class FormatDelegate : public QStyledItemDelegate {
    QWidget* createEditor(QWidget* parent, ...) override {
        auto* combo = new QComboBox(parent);
        combo->addItems({"mp3","flac","wav","aiff","alac","ogg","m4a"});
        return combo;  // created on demand, destroyed after edit
    }
};
```

### Drag and drop (opt-in only)

```cpp
// Nothing drags unless explicitly enabled
TrackTableView::TrackTableView() : QTableView() {
    setDragEnabled(false);       // default — no accidental drag
    setAcceptDrops(false);
    // Enable only for playlist reorder if needed later:
    // setDragDropMode(QAbstractItemView::InternalMove);
}

// For import: accept .txt file drops on the playlist panel
PlaylistPanel::PlaylistPanel() {
    setAcceptDrops(true);        // only this widget accepts drops
}
void PlaylistPanel::dropEvent(QDropEvent* e) {
    if (e->mimeData()->hasUrls()) {
        for (auto& url : e->mimeData()->urls())
            if (url.toLocalFile().endsWith(".txt"))
                emit importRequested(url.toLocalFile());
    }
}
```

### Keyboard shortcuts and command system

```cpp
// All actions declared once, reused in menu + toolbar + keyboard
MainWindow::MainWindow() {
    undoStack = new QUndoStack(this);

    auto* undoAct = undoStack->createUndoAction(this, tr("&Undo"));
    undoAct->setShortcut(QKeySequence::Undo);  // Ctrl+Z on Win, Cmd+Z on Mac

    auto* deleteAct = new QAction(tr("Delete"), this);
    deleteAct->setShortcut(QKeySequence::Delete);
    deleteAct->setShortcutContext(Qt::WindowShortcut);
    connect(deleteAct, &QAction::triggered, this, &MainWindow::deleteSelected);
    addAction(deleteAct);

    auto* editMenu = menuBar()->addMenu(tr("&Edit"));
    editMenu->addAction(undoAct);
    editMenu->addSeparator();
    editMenu->addAction(deleteAct);
}

// Every mutation goes through QUndoCommand
class UpdateFormatCommand : public QUndoCommand {
    TrackModel* model;
    QPersistentModelIndex index;
    QString oldFormat, newFormat;
public:
    UpdateFormatCommand(TrackModel* m, QModelIndex idx, QString fmt)
        : model(m), index(idx), newFormat(fmt) {
        oldFormat = m->data(idx, TrackModel::FormatRole).toString();
        setText(tr("Change format to %1").arg(fmt));
    }
    void redo() override { model->setData(index, newFormat, TrackModel::FormatRole); }
    void undo() override { model->setData(index, oldFormat, TrackModel::FormatRole); }
};

// Usage
undoStack->push(new UpdateFormatCommand(trackModel, index, "aiff"));
```

### High-DPI and font rendering

```cpp
// main.cpp (Qt5 — Qt6 is automatic)
QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);

// Custom painting — always logical pixels
void MyWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.drawRect(0, 0, width(), height()); // logical — Qt scales automatically
}

// Icons — SVG or multi-resolution
QIcon icon;
icon.addFile(":/icons/delete_16.png", {16,16});
icon.addFile(":/icons/delete_32.png", {32,32});

// Fonts — point sizes, never pixel sizes
QFont f = QApplication::font();  // Segoe UI on Windows
f.setPointSize(10);
```

### Large track lists — virtualization

```cpp
class TrackModel : public QAbstractTableModel {
    QVector<Track> cache;
    int totalCount = 0, loadedUpTo = 0;

    bool canFetchMore(const QModelIndex&) const override {
        return loadedUpTo < totalCount;
    }
    void fetchMore(const QModelIndex&) override {
        int batch = qMin(200, totalCount - loadedUpTo);
        beginInsertRows({}, loadedUpTo, loadedUpTo + batch - 1);
        db->loadTracks(loadedUpTo, batch, cache); // SELECT ... LIMIT 200 OFFSET n
        loadedUpTo += batch;
        endInsertRows();
    }
};

TrackTableView::TrackTableView() {
    setUniformRowHeights(true);     // massive perf win — avoids per-row height calc
    setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);  // smooth
    setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    setSortingEnabled(true);

    // Filtering via proxy — never touch source model directly
    auto* proxy = new QSortFilterProxyModel(this);
    proxy->setSourceModel(trackModel);
    proxy->setFilterCaseSensitivity(Qt::CaseInsensitive);
    proxy->setFilterKeyColumn(-1); // search all columns
    setModel(proxy);
}
```

### Threading model

```cpp
// Worker object pattern — don't subclass QThread
class ConversionWorker : public QObject {
    Q_OBJECT
public slots:
    void convert(ConversionJob job) {
        // runs on worker thread — blocking QProcess call is fine here
        QProcess ffmpeg;
        ffmpeg.start("ffmpeg", buildArgs(job));
        ffmpeg.waitForFinished(-1);
        emit finished(job.id, ffmpeg.exitCode() == 0);
    }
signals:
    void finished(qint64 id, bool success);
    void progress(qint64 id, int percent);
};

// Setup
auto* worker = new ConversionWorker();
auto* thread = new QThread(this);
worker->moveToThread(thread);
thread->start();

// Cross-thread signal — automatically Qt::QueuedConnection, safe to update UI
connect(worker, &ConversionWorker::finished,
        this, &MainWindow::onConversionDone);

// Dispatch to worker thread
QMetaObject::invokeMethod(worker, "convert",
    Qt::QueuedConnection, Q_ARG(ConversionJob, job));
```

---

## 6. Cross-Platform (Windows + macOS from one codebase)

**One codebase, two builds.** Qt abstracts the OS at the API level.

```cmake
# CMakeLists.txt — same file for both platforms
cmake_minimum_required(VERSION 3.21)
project(Ordnung)
find_package(Qt6 REQUIRED COMPONENTS Core Widgets Sql Concurrent)

add_executable(Ordnung
    src/main.cpp
    src/app/MainWindow.cpp
    src/models/TrackModel.cpp
    # ... same list on both platforms
)
target_link_libraries(Ordnung Qt6::Core Qt6::Widgets Qt6::Sql Qt6::Concurrent)

# Platform-specific bundle config only
if(WIN32)
    set_target_properties(Ordnung PROPERTIES WIN32_EXECUTABLE TRUE)
endif()
if(APPLE)
    set_target_properties(Ordnung PROPERTIES MACOSX_BUNDLE TRUE)
endif()
```

| Step | Windows | macOS |
|---|---|---|
| Bundle Qt | `windeployqt ordnung.exe` | `macdeployqt Ordnung.app` |
| Installer | `.msi` (WiX) or NSIS | `.dmg` |
| Shortcuts | `Ctrl+Z`, `Ctrl+S` | `Cmd+Z`, `Cmd+S` — Qt maps automatically |
| Menu bar | Inside window | System menu bar — Qt handles this |
| Native style | Windows 11 | macOS Aqua |

### GitHub Actions CI

```yaml
jobs:
  build-windows:
    runs-on: windows-latest
    steps:
      - uses: jurplel/install-qt-action@v3
        with: { version: '6.7.0' }
      - run: cmake -B build && cmake --build build --config Release
      - run: windeployqt build/Release/Ordnung.exe

  build-macos:
    runs-on: macos-latest
    steps:
      - uses: jurplel/install-qt-action@v3
        with: { version: '6.7.0' }
      - run: cmake -B build && cmake --build build
      - run: macdeployqt build/Ordnung.app -dmg
```

---

## 7. Qt License

| License | Cost | Constraint |
|---|---|---|
| **LGPL** (open source) | Free | Dynamic link Qt (default) = no restriction on app license |
| **Commercial** | ~$500/mo per dev | No restrictions, includes Qt Design Studio |

For most projects: **LGPL + dynamic linking**. `windeployqt`/`macdeployqt` bundles the Qt DLLs/frameworks automatically, keeping you compliant at zero cost.

---

## 8. Common Mistakes That Make Qt Apps Feel Cheap or Webby

### Interaction model mistakes
- **`Qt::TextSelectableByMouse` on every `QLabel`** — users can drag-select the whole UI like a webpage. Set `Qt::NoTextInteraction` on all display-only labels explicitly.
- **Embedding `QWidget`s in table cells** (`setCellWidget`) — looks fine at 50 rows, breaks at 1,000+. Use delegates.
- **Not implementing tab order** — `QWidget::setTabOrder()` must be chained. Without it, Tab key does nothing useful.
- **Not setting `setFocusPolicy(Qt::StrongFocus)`** on custom widgets that should receive keyboard input.

### Visual mistakes
- **Hardcoding colors** instead of `QPalette` — broken in Windows dark mode or high-contrast accessibility themes.
- **Global `setStyleSheet`** overriding native drawing on parts you didn't intend. Scope stylesheets to specific widgets.
- **Fixed pixel sizes** anywhere — layouts, fonts, margins. Use `fontMetrics().height()` as spacing unit.
- **Not providing multi-resolution `QIcon`** — blurry icons on 4K/HiDPI.

### Architecture mistakes
- **Skipping `QUndoStack`** — the #1 thing that makes an app feel unfinished.
- **Calling `QWidget` methods from a worker thread** — silent corruption or crash. Always use `Qt::QueuedConnection`.
- **Subclassing `QThread` and putting logic in `run()`** — the old antipattern. Use `moveToThread()` with a worker `QObject`.
- **Using `QListWidget`/`QTableWidget`** instead of `QAbstractItemModel` + `QTableView` — convenient for 50 items, disaster for 50,000. `QTableWidget` is just `QTableView` with a built-in model; it is not scalable.
- **Not using `QSortFilterProxyModel`** — reimplementing search/filter manually always has edge cases.

### Cross-platform quality
- On **macOS**: `QMenuBar` merges into the system menu bar. Don't replace it with a custom toolbar.
- On **Windows**: don't override `paintEvent` on standard controls — let `QWindowsVistaStyle` do its job.
- **Font rendering**: set `QFont::PreferAntialias` and test on both standard and ClearType displays.
