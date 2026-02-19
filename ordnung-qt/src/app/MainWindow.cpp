#include "app/MainWindow.h"

#include "views/LibraryView.h"
#include "views/DownloadsView.h"
#include "models/PlaylistModel.h"
#include "models/TrackModel.h"
#include "models/DownloadsModel.h"
#include "services/Database.h"
#include "services/Converter.h"
#include "services/FolderWatcher.h"
#include "services/PlaylistImporter.h"

#include <QWidget>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QStackedWidget>
#include <QPushButton>
#include <QLabel>
#include <QUndoStack>
#include <QThread>
#include <QDateTime>
#include <QFileInfo>
#include <QMessageBox>
#include <QMetaObject>
#include <QStyle>

MainWindow::MainWindow(const QString& themeSheet, QWidget* parent)
    : QMainWindow(parent)
{
    setMinimumSize(1100, 700);
    setWindowTitle("eyebags terminal");

    // Apply QSS scoped to this window (per DESIGN_SPEC §9)
    if (!themeSheet.isEmpty())
        setStyleSheet(themeSheet);

    setupServices();

    // ── Central widget ───────────────────────────────────────────────────────
    auto* central = new QWidget(this);
    setCentralWidget(central);

    auto* rootLayout = new QHBoxLayout(central);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    // ── Sidebar ──────────────────────────────────────────────────────────────
    auto* sidebar = new QWidget(central);
    sidebar->setObjectName("sidebar");
    sidebar->setFixedWidth(220);
    buildSidebar(sidebar);
    rootLayout->addWidget(sidebar);

    // ── Content stack ────────────────────────────────────────────────────────
    m_stack = new QStackedWidget(central);

    m_libraryView  = new LibraryView(m_playlists, m_tracks, m_db, m_undoStack, m_stack);
    m_downloadsView = new DownloadsView(m_downloads, m_stack);

    m_stack->addWidget(m_libraryView);    // index 0
    m_stack->addWidget(m_downloadsView);  // index 1

    rootLayout->addWidget(m_stack, 1);

    // ── Signal wiring ────────────────────────────────────────────────────────
    connect(m_libraryView, &LibraryView::importRequested,
            this, &MainWindow::onImportRequested);
    connect(m_libraryView, &LibraryView::deletePlaylistRequested,
            this, &MainWindow::onDeletePlaylist);

    connect(m_downloadsView, &DownloadsView::saveConfigRequested,
            this, &MainWindow::onSaveConfig);
    connect(m_downloadsView, &DownloadsView::scanRequested,
            this, &MainWindow::onScanRequested);
    connect(m_downloadsView, &DownloadsView::convertAllRequested,
            this, &MainWindow::onConvertAll);
    connect(m_downloadsView, &DownloadsView::convertSingleRequested,
            this, &MainWindow::onConvertSingle);
    connect(m_downloadsView, &DownloadsView::deleteDownloadRequested,
            this, &MainWindow::onDeleteDownload);

    // Converter signals (cross-thread — Qt::QueuedConnection is automatic)
    connect(m_converter, &ConversionWorker::conversionStarted,
            this, &MainWindow::onConversionStarted);
    connect(m_converter, &ConversionWorker::conversionFinished,
            this, &MainWindow::onConversionFinished);
    connect(m_converter, &ConversionWorker::logLine,
            this, &MainWindow::onWorkerLog);
    connect(m_watcher, &FolderWatcher::logLine,
            this, &MainWindow::onWorkerLog);

    restoreState();
}

MainWindow::~MainWindow()
{
    m_workerThread->quit();
    m_workerThread->wait();
}

void MainWindow::setupServices()
{
    // Database
    m_db = new Database(this);
    if (!m_db->open()) {
        QMessageBox::critical(nullptr, "Database error",
            "Could not open database:\n" + m_db->errorString());
        // Continue — UI will be empty but not crashed
    }

    // Models
    m_playlists = new PlaylistModel(m_db, this);
    m_tracks    = new TrackModel(m_db, this);
    m_downloads = new DownloadsModel(m_db, this);
    m_undoStack = new QUndoStack(this);

    // Converter on its own thread (worker-object pattern)
    m_converter    = new ConversionWorker(m_db);
    m_workerThread = new QThread(this);
    m_converter->moveToThread(m_workerThread);
    m_workerThread->start();

    // Folder watcher (runs on main thread — uses Qt's async QFileSystemWatcher)
    m_watcher = new FolderWatcher(m_db, this);

    // Importer (stateless, created once, used per import)
    m_importer = new PlaylistImporter(this);
}

void MainWindow::buildSidebar(QWidget* sidebar)
{
    // Logo
    auto* logo = new QLabel("eyebags\nterminal", sidebar);
    logo->setObjectName("logoLabel");
    logo->setTextInteractionFlags(Qt::NoTextInteraction);
    logo->setAlignment(Qt::AlignLeft | Qt::AlignBottom);
    {
        QFont f = logo->font();
        f.setPointSize(20);
        f.setWeight(QFont::Light);
        f.setLetterSpacing(QFont::AbsoluteSpacing, 3.0);
        logo->setFont(f);
    }

    // Nav buttons
    m_libNavBtn = new QPushButton("library", sidebar);
    m_libNavBtn->setObjectName("navBtn");
    m_libNavBtn->setFocusPolicy(Qt::NoFocus);

    m_dlNavBtn = new QPushButton("downloads", sidebar);
    m_dlNavBtn->setObjectName("navBtn");
    m_dlNavBtn->setFocusPolicy(Qt::NoFocus);

    connect(m_libNavBtn, &QPushButton::clicked, this, &MainWindow::switchToLibrary);
    connect(m_dlNavBtn,  &QPushButton::clicked, this, &MainWindow::switchToDownloads);

    // Version footer
    auto* version = new QLabel("v1.0.0", sidebar);
    version->setObjectName("versionLabel");
    version->setTextInteractionFlags(Qt::NoTextInteraction);

    auto* layout = new QVBoxLayout(sidebar);
    layout->setContentsMargins(0, 23, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(logo);
    layout->addWidget(m_libNavBtn);
    layout->addWidget(m_dlNavBtn);
    layout->addStretch();
    layout->addWidget(version);

    // Start on library
    setNavActive(m_libNavBtn);
}

void MainWindow::restoreState()
{
    // Load playlists
    m_playlists->reload();

    // Load watch config and apply to downloads view
    const WatchConfig cfg = m_db->loadWatchConfig();
    m_downloadsView->setWatchConfig(cfg);

    // Restore folder watcher
    if (!cfg.watchFolder.isEmpty())
        m_watcher->setFolder(cfg.watchFolder);

    // Reload downloads
    m_downloads->reload();

    // Select first playlist if any
    if (m_playlists->rowCount() > 0) {
        const Playlist* first = m_playlists->playlistAt(0);
        if (first)
            m_libraryView->loadPlaylist(first->id);
    }
}

void MainWindow::setNavActive(QPushButton* active)
{
    for (auto* btn : {m_libNavBtn, m_dlNavBtn}) {
        if (!btn) continue;
        btn->setProperty("active", btn == active);
        btn->style()->unpolish(btn);
        btn->style()->polish(btn);
    }
}

void MainWindow::switchToLibrary()
{
    m_stack->setCurrentIndex(0);
    setNavActive(m_libNavBtn);
}

void MainWindow::switchToDownloads()
{
    m_stack->setCurrentIndex(1);
    setNavActive(m_dlNavBtn);
}

// ── Import ───────────────────────────────────────────────────────────────────

void MainWindow::onImportRequested(const QStringList& paths)
{
    for (const QString& path : paths) {
        const ImportResult result = m_importer->parse(path);
        if (!result.ok) {
            m_downloadsView->appendLogLine(
                QString("Import failed: %1 — %2").arg(QFileInfo(path).fileName(), result.error));
            continue;
        }

        // Derive playlist name from filename (strip .txt extension)
        QString name = QFileInfo(path).completeBaseName();
        const QString ts = QDateTime::currentDateTime().toString(Qt::ISODate);

        const long long playlistId = m_db->insertPlaylist(name, ts);
        if (playlistId < 0) {
            m_downloadsView->appendLogLine(
                QString("Import failed: could not create playlist for %1").arg(name));
            continue;
        }

        int added = 0;
        for (const Track& t : result.tracks) {
            const long long songId = m_db->upsertSong(t);
            if (songId > 0) {
                m_db->linkSongToPlaylist(songId, playlistId);
                ++added;
            }
        }

        m_downloadsView->appendLogLine(
            QString("Imported '%1': %2 tracks").arg(name).arg(added));
    }

    m_libraryView->reloadPlaylists();
}

void MainWindow::onDeletePlaylist(long long id)
{
    m_db->deletePlaylist(id);
    m_libraryView->reloadPlaylists();
    m_tracks->clear();
}

// ── Config & folder ──────────────────────────────────────────────────────────

void MainWindow::onSaveConfig(const WatchConfig& cfg)
{
    m_db->saveWatchConfig(cfg);
    if (!cfg.watchFolder.isEmpty())
        m_watcher->setFolder(cfg.watchFolder);
    m_downloadsView->appendLogLine("Config saved.");
}

void MainWindow::onScanRequested(const QString& folder)
{
    if (folder.isEmpty()) {
        m_downloadsView->appendLogLine("No source folder set.");
        return;
    }
    const QString ts = QDateTime::currentDateTime().toString(Qt::ISODate);
    const auto result = m_watcher->scan(folder, ts);
    m_downloadsView->reloadTable();
    m_downloadsView->appendLogLine(
        QString("Scan complete: %1 files found, %2 new.").arg(result.scanned).arg(result.added));
}

// ── Conversion ───────────────────────────────────────────────────────────────

void MainWindow::onConvertAll(const QString& outputFolder)
{
    if (outputFolder.isEmpty()) {
        m_downloadsView->appendLogLine("No output folder set.");
        return;
    }
    int queued = 0;
    const int rows = m_downloads->rowCount();
    for (int i = 0; i < rows; ++i) {
        const Download* dl = m_downloads->downloadAt(i);
        if (!dl) continue;
        // Only queue files that aren't already done or converting
        if (dl->conv_status == ConversionStatus::Done ||
            dl->conv_status == ConversionStatus::Converting)
            continue;
        QMetaObject::invokeMethod(m_converter, "enqueue",
            Qt::QueuedConnection,
            Q_ARG(long long, dl->id),
            Q_ARG(QString,   QString::fromStdString(dl->filepath)),
            Q_ARG(QString,   outputFolder));
        ++queued;
    }
    m_downloadsView->appendLogLine(
        QString("Queued %1 files for conversion.").arg(queued));
}

void MainWindow::onConvertSingle(long long downloadId, const QString& sourcePath,
                                 const QString& outputFolder)
{
    if (outputFolder.isEmpty()) {
        m_downloadsView->appendLogLine("No output folder set.");
        return;
    }
    QMetaObject::invokeMethod(m_converter, "enqueue",
        Qt::QueuedConnection,
        Q_ARG(long long, downloadId),
        Q_ARG(QString,   sourcePath),
        Q_ARG(QString,   outputFolder));
}

void MainWindow::onDeleteDownload(long long id)
{
    m_db->deleteDownload(id);
    m_downloads->removeRow(id);
}

void MainWindow::onConversionStarted(long long convId, long long downloadId)
{
    m_downloadsView->onConversionUpdate(downloadId, convId, ConversionStatus::Converting, {});
}

void MainWindow::onConversionFinished(long long convId, long long downloadId,
                                      bool success, const QString& error)
{
    const ConversionStatus status = success ? ConversionStatus::Done : ConversionStatus::Failed;
    m_downloadsView->onConversionUpdate(downloadId, convId, status, error);
}

void MainWindow::onWorkerLog(const QString& line)
{
    m_downloadsView->appendLogLine(line);
}
