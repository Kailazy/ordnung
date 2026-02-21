#include "views/LibraryView.h"

#include "views/CollectionTreePanel.h"
#include "views/TrackTableView.h"
#include "views/TrackDetailPanel.h"
#include "views/PlayerBar.h"
#include "views/ExportWizard.h"
#include "views/AnalysisProgressDialog.h"
#include "views/BatchEditDialog.h"
#include "views/MissingFilesDialog.h"
#include "views/DuplicateDetectorDialog.h"
#include "services/M3UExporter.h"
#include "models/TrackModel.h"
#include "commands/UpdateFormatCommand.h"
#include "services/Database.h"
#include "services/LibraryScanner.h"
#include "services/PlaylistImporter.h"
#include "services/AudioAnalyzer.h"
#include "style/Theme.h"

#include <QtConcurrent/QtConcurrent>
#include <QSet>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QSplitter>
#include <QSizePolicy>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QAction>
#include <QMenu>
#include <QItemSelectionModel>
#include <QUndoStack>
#include <QFileDialog>
#include <QDir>
#include <QFileInfo>
#include <QDateTime>
#include <QMessageBox>
#include <QInputDialog>
#include <QDebug>

static QWidget* makeSep(QWidget* parent)
{
    auto* sep = new QWidget(parent);
    sep->setFixedHeight(1);
    sep->setStyleSheet(QString("background-color: %1;").arg(Theme::Color::Border));
    sep->setAttribute(Qt::WA_StyledBackground, true);
    return sep;
}

LibraryView::LibraryView(TrackModel* tracks, Database* db,
                         QUndoStack* undoStack, QWidget* parent)
    : QWidget(parent)
    , m_trackModel(tracks)
    , m_db(db)
    , m_undoStack(undoStack)
{
    // ── Toolbar ──────────────────────────────────────────────────────────────
    auto* toolbar = new QWidget(this);
    toolbar->setObjectName("toolbar");
    toolbar->setFixedHeight(Theme::Layout::ToolbarH);

    m_folderBtn = new QPushButton("library", toolbar);
    m_folderBtn->setObjectName("libraryToggleBtn");
    m_folderBtn->setCursor(Qt::PointingHandCursor);
    connect(m_folderBtn, &QPushButton::clicked, this, &LibraryView::onBrowseFolderClicked);

    m_searchEdit = new QLineEdit(toolbar);
    m_searchEdit->setObjectName("searchInput");
    m_searchEdit->setPlaceholderText("search tracks...");
    m_searchEdit->setClearButtonEnabled(true);
    m_searchEdit->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    m_searchEdit->setMinimumWidth(160);
    m_searchEdit->setMaximumWidth(280);

    m_undoBtn = new QPushButton("undo", toolbar);
    m_undoBtn->setProperty("btnStyle", "undo");
    m_undoBtn->setVisible(false);

    m_exportBtn = new QPushButton("export", toolbar);
    m_exportBtn->setObjectName("exportBtn");
    m_exportBtn->setCursor(Qt::PointingHandCursor);
    connect(m_exportBtn, &QPushButton::clicked, this, &LibraryView::onExportClicked);

    m_analyzeBtn = new QPushButton("analyze", toolbar);
    m_analyzeBtn->setObjectName("analyzeBtn");
    m_analyzeBtn->setCursor(Qt::PointingHandCursor);
    connect(m_analyzeBtn, &QPushButton::clicked, this, &LibraryView::onAnalyzeClicked);

    m_searchBadge = new QLabel(toolbar);
    m_searchBadge->setObjectName("searchResultBadge");
    m_searchBadge->setVisible(false);
    m_searchBadge->setStyleSheet(
        QString("background: %1; color: %2; font-size: %3px; "
                "padding: 2px 8px; font-family: '%4', '%5';")
            .arg(Theme::Color::AccentBg, Theme::Color::Accent)
            .arg(Theme::Font::Badge)
            .arg(Theme::Font::Mono, Theme::Font::MonoFallback));

    m_editSelectedBtn = new QPushButton("EDIT SELECTED", toolbar);
    m_editSelectedBtn->setObjectName("editSelectedBtn");
    m_editSelectedBtn->setCursor(Qt::PointingHandCursor);
    m_editSelectedBtn->setEnabled(false);
    connect(m_editSelectedBtn, &QPushButton::clicked, this, &LibraryView::onEditSelectedClicked);

    m_missingBtn = new QPushButton("missing", toolbar);
    m_missingBtn->setObjectName("missingBtn");
    m_missingBtn->setCursor(Qt::PointingHandCursor);
    connect(m_missingBtn, &QPushButton::clicked, this, &LibraryView::onFindMissingClicked);

    m_duplicatesBtn = new QPushButton("duplicates", toolbar);
    m_duplicatesBtn->setObjectName("missingBtn");
    m_duplicatesBtn->setCursor(Qt::PointingHandCursor);
    connect(m_duplicatesBtn, &QPushButton::clicked, this, &LibraryView::onDuplicatesClicked);

    m_statsLabel = new QLabel(toolbar);
    m_statsLabel->setObjectName("statsLabel");
    m_statsLabel->setTextInteractionFlags(Qt::NoTextInteraction);

    auto* toolbarLayout = new QHBoxLayout(toolbar);
    toolbarLayout->setContentsMargins(Theme::Layout::ContentPadH, 0,
                                      Theme::Layout::ContentPadH, 0);
    toolbarLayout->setSpacing(Theme::Layout::Gap);
    toolbarLayout->addWidget(m_folderBtn);
    toolbarLayout->addWidget(m_searchEdit);
    toolbarLayout->addWidget(m_searchBadge);
    toolbarLayout->addWidget(m_undoBtn);
    toolbarLayout->addWidget(m_exportBtn);
    toolbarLayout->addWidget(m_analyzeBtn);
    toolbarLayout->addWidget(m_editSelectedBtn);
    toolbarLayout->addWidget(m_missingBtn);
    toolbarLayout->addWidget(m_duplicatesBtn);
    toolbarLayout->addStretch();
    toolbarLayout->addWidget(m_statsLabel);

    // ── CollectionTreePanel — permanent left pane ─────────────────────────
    m_collectionPanel = new CollectionTreePanel(db, this);
    m_collectionPanel->setFixedWidth(Theme::Layout::PlaylistW);

    // ── Track panels ──────────────────────────────────────────────────────
    m_trackTable  = new TrackTableView(tracks, undoStack, this);
    m_trackTable->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_detailPanel = new TrackDetailPanel(db, this);
    m_detailPanel->setVisible(false);

    auto* vSplitter = new QSplitter(Qt::Vertical, this);
    vSplitter->addWidget(m_trackTable);
    vSplitter->addWidget(m_detailPanel);
    vSplitter->setCollapsible(0, false);
    vSplitter->setCollapsible(1, true);
    vSplitter->setSizes({600, 200});
    vSplitter->setHandleWidth(1);

    // ── Horizontal split: collection tree | track area ────────────────────
    auto* hSplitter = new QSplitter(Qt::Horizontal, this);
    hSplitter->addWidget(m_collectionPanel);
    hSplitter->addWidget(vSplitter);
    hSplitter->setCollapsible(0, false);
    hSplitter->setCollapsible(1, false);
    hSplitter->setHandleWidth(1);

    // ── Player bar ────────────────────────────────────────────────────────
    m_playerBar = new PlayerBar(this);
    m_playerBar->setVisible(false);

    // ── Root layout ───────────────────────────────────────────────────────
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);
    root->addWidget(toolbar);
    root->addWidget(makeSep(this));
    root->addWidget(hSplitter, 1);
    root->addWidget(makeSep(this));
    root->addWidget(m_playerBar);

    // ── Connections ───────────────────────────────────────────────────────
    connect(m_searchEdit, &QLineEdit::textChanged,
            this, [this](const QString& text) {
                const QString query = text.trimmed();
                if (query.isEmpty()) {
                    // Reset to proxy filter mode (show all)
                    m_trackTable->setSearchText({});
                    m_searchBadge->setVisible(false);
                } else {
                    m_trackModel->searchFts(query);
                    const int count = m_trackModel->rowCount();
                    m_searchBadge->setText(QString("%1 results").arg(count));
                    m_searchBadge->setVisible(true);
                }
                updateStats();
            });

    // Track table multi-selection for batch edit
    connect(m_trackTable->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, [this]() { onSelectionChanged(); });

    // Context menu is handled by TrackTableView::contextMenuEvent override.
    // prepareToggleRequested and batchEditRequested are connected below.
    connect(m_undoBtn, &QPushButton::clicked, m_undoStack, &QUndoStack::undo);
    connect(m_undoStack, &QUndoStack::canUndoChanged,
            this, &LibraryView::onUndoAvailable);

    // CollectionTreePanel
    connect(m_collectionPanel, &CollectionTreePanel::collectionSelected,
            this, &LibraryView::onCollectionSelected);
    connect(m_collectionPanel, &CollectionTreePanel::playlistSelected,
            this, &LibraryView::onPlaylistSelected);
    connect(m_collectionPanel, &CollectionTreePanel::smartPlaylistSelected,
            this, &LibraryView::onSmartPlaylistSelected);
    connect(m_collectionPanel, &CollectionTreePanel::importRequested,
            this, &LibraryView::onImportRequested);
    connect(m_collectionPanel, &CollectionTreePanel::createPlaylistRequested,
            this, &LibraryView::onCreatePlaylistRequested);
    connect(m_collectionPanel, &CollectionTreePanel::deletePlaylistRequested,
            this, &LibraryView::onDeletePlaylistRequested);
    connect(m_collectionPanel, &CollectionTreePanel::exportPlaylistRequested,
            this, &LibraryView::onExportPlaylistRequested);
    connect(m_collectionPanel, &CollectionTreePanel::historyDateSelected,
            this, &LibraryView::onHistoryDateSelected);
    connect(m_collectionPanel, &CollectionTreePanel::exportPlaylistM3uRequested,
            this, &LibraryView::onExportPlaylistM3uRequested);

    connect(m_trackTable, &TrackTableView::trackExpanded,
            this,         &LibraryView::onTrackExpanded);
    connect(m_trackTable, &TrackTableView::trackCollapsed,
            this,         &LibraryView::onTrackCollapsed);
    connect(m_trackTable, &TrackTableView::formatChangeRequested,
            this, [this](const QModelIndex& srcIdx, const QString& fmt) {
                m_undoStack->push(new UpdateFormatCommand(m_trackModel, m_db, srcIdx, fmt));
            });
    connect(m_trackTable,  &TrackTableView::playRequested,
            this,          &LibraryView::onPlayRequested);
    connect(m_trackTable, &TrackTableView::prepareToggleRequested,
            this,         &LibraryView::onPrepareToggleRequested);
    connect(m_trackTable, &TrackTableView::batchEditRequested,
            this,         &LibraryView::onEditSelectedClicked);
    connect(m_detailPanel, &TrackDetailPanel::playRequested,
            this,          &LibraryView::onPlayRequested);

    connect(m_detailPanel, &TrackDetailPanel::aiffToggled,
            this, [this](long long songId, bool newValue) {
                m_db->updateSongAiff(songId, newValue);
                const int row = m_trackModel->rowForId(songId);
                if (row >= 0)
                    m_trackModel->setHasAiff(row, newValue);
            });

    connect(m_detailPanel, &TrackDetailPanel::playlistMembershipChanged,
            this, [this](long long songId, long long playlistId, bool added) {
                if (added)
                    m_db->addSongToPlaylist(songId, playlistId);
                else
                    m_db->removeSongFromPlaylist(songId, playlistId);
            });
}

// ─────────────────────────────────────────────────────────────────────────────
// Public API
// ─────────────────────────────────────────────────────────────────────────────

void LibraryView::setLibraryFolder(const QString& path)
{
    m_libraryFolder = path;
    if (path.isEmpty()) {
        m_folderBtn->setText("library");
        m_folderBtn->setToolTip(QString());
    } else {
        const QString name = QFileInfo(path).fileName();
        m_folderBtn->setText(name.isEmpty() ? path : name);
        m_folderBtn->setToolTip(path);
    }
    if (!path.isEmpty())
        loadAndScan();
}

// ─────────────────────────────────────────────────────────────────────────────
// Private helpers
// ─────────────────────────────────────────────────────────────────────────────

void LibraryView::loadAndScan()
{
    if (m_libraryFolder.isEmpty()) return;

    const QVector<Track> dbTracks = m_db->loadLibrarySongs(m_libraryFolder);
    qInfo() << "[Library] Loaded" << dbTracks.size() << "tracks from DB";
    m_trackModel->loadFromDatabase(dbTracks);
    m_detailPanel->clear();
    m_trackTable->setSearchText({});
    m_searchEdit->clear();
    updateStats();
    rescan();
}

void LibraryView::rescan()
{
    if (m_libraryFolder.isEmpty()) return;
    if (m_scanWatcher && m_scanWatcher->isRunning()) return;

    const QVector<Track> known = m_db->loadLibrarySongs(m_libraryFolder);
    QSet<QString> knownPaths;
    knownPaths.reserve(known.size());
    for (const Track& t : known)
        knownPaths.insert(QString::fromStdString(t.filepath));

    qInfo() << "[Library] Scanning for new files in:" << m_libraryFolder
            << "(already tracked:" << knownPaths.size() << ")";

    if (!m_scanWatcher) {
        m_scanWatcher = new QFutureWatcher<QVector<Track>>(this);
        connect(m_scanWatcher, &QFutureWatcher<QVector<Track>>::finished,
                this, &LibraryView::onScanFinished);
    }

    const QString folder = m_libraryFolder;
    m_scanWatcher->setFuture(QtConcurrent::run([folder, knownPaths]() -> QVector<Track> {
        // Fast scan: filename-only, no ffprobe. Tracks appear in table immediately.
        // AudioAnalyzer::analyzeLibrary() fills in BPM/key/bitrate in the background.
        const QVector<Track> all = LibraryScanner::scanFast(folder);
        QVector<Track> newOnes;
        for (const Track& t : all) {
            if (!knownPaths.contains(QString::fromStdString(t.filepath)))
                newOnes.append(t);
        }
        return newOnes;
    }));
}

void LibraryView::importPlaylistFile(const QString& filePath)
{
    PlaylistImporter importer(this);
    const ImportResult result = importer.parse(filePath);
    if (!result.ok) {
        QMessageBox::warning(this, "Import Failed", result.error);
        return;
    }

    const QString name = QFileInfo(filePath).completeBaseName();
    const QString now  = QDateTime::currentDateTime().toString(Qt::ISODate);
    const long long id = m_db->insertPlaylist(name, now);
    if (id < 0) {
        QMessageBox::warning(this, "Import Failed", "Could not create playlist in database.");
        return;
    }

    for (const Track& t : result.tracks) {
        const long long songId = m_db->upsertSong(t);
        if (songId > 0)
            m_db->linkSongToPlaylist(songId, id);
    }

    m_collectionPanel->reloadPlaylists();
    qInfo() << "[Library] Imported playlist:" << name
            << "(" << result.tracks.size() << "tracks)";
}

void LibraryView::updateStats()
{
    const int filtered = m_trackTable->proxy()->rowCount();
    const int total    = m_trackModel->rowCount();
    if (filtered == total)
        m_statsLabel->setText(QString("%1 tracks").arg(total));
    else
        m_statsLabel->setText(QString("%1 / %2").arg(filtered).arg(total));
}

// ─────────────────────────────────────────────────────────────────────────────
// Slots
// ─────────────────────────────────────────────────────────────────────────────

void LibraryView::onBrowseFolderClicked()
{
    const QString dir = QFileDialog::getExistingDirectory(
        this,
        "Select Library Folder",
        m_libraryFolder.isEmpty() ? QDir::homePath() : m_libraryFolder,
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    if (dir.isEmpty()) return;

    m_libraryFolder = dir;
    setLibraryFolder(dir);
    emit libraryFolderChanged(dir);
}

void LibraryView::onCollectionSelected()
{
    if (m_libraryFolder.isEmpty()) return;
    m_activePlaylistId = -1;
    const QVector<Track> tracks = m_db->loadLibrarySongs(m_libraryFolder);
    m_trackModel->loadFromDatabase(tracks);
    m_detailPanel->clear();
    updateStats();
}

void LibraryView::onPlaylistSelected(long long id)
{
    m_activePlaylistId = id;
    m_trackModel->loadPlaylist(id);
    m_detailPanel->clear();
    updateStats();
}

void LibraryView::onSmartPlaylistSelected(const QString& key)
{
    m_activePlaylistId = -1;
    m_detailPanel->clear();

    QVector<Track> tracks;
    if (key == QStringLiteral("needs_aiff")) {
        const QVector<Track> all = m_db->loadLibrarySongs(m_libraryFolder);
        for (const Track& t : all)
            if (!t.has_aiff) tracks.append(t);
    } else if (key == QStringLiteral("high_bpm")) {
        const QVector<Track> all = m_db->loadLibrarySongs(m_libraryFolder);
        for (const Track& t : all)
            if (t.bpm > 140.0) tracks.append(t);
    } else if (key == QStringLiteral("top_rated")) {
        const QVector<Track> all = m_db->loadLibrarySongs(m_libraryFolder);
        for (const Track& t : all)
            if (t.rating >= 3) tracks.append(t);
    } else if (key == QStringLiteral("prepared")) {
        tracks = m_db->loadPreparedTracks();
    } else if (key == QStringLiteral("recently_added")) {
        tracks = m_db->loadRecentlyAdded(30);
    } else if (key == QStringLiteral("recently_played")) {
        tracks = m_db->loadRecentlyPlayed(50);
    } else {
        onCollectionSelected();
        return;
    }

    m_trackModel->loadFromDatabase(tracks);
    updateStats();
}

void LibraryView::onImportRequested(const QStringList& filePaths)
{
    QStringList paths = filePaths;
    if (paths.isEmpty()) {
        paths = QFileDialog::getOpenFileNames(
            this,
            "Import Rekordbox Playlist",
            QDir::homePath(),
            "Rekordbox Export (*.txt);;All Files (*)");
    }
    for (const QString& p : paths)
        importPlaylistFile(p);
}

void LibraryView::onCreatePlaylistRequested()
{
    bool ok = false;
    const QString name = QInputDialog::getText(
        this, "New Playlist", "Playlist name:", QLineEdit::Normal, QString(), &ok);
    if (!ok || name.trimmed().isEmpty()) return;

    const QString now = QDateTime::currentDateTime().toString(Qt::ISODate);
    const long long id = m_db->insertPlaylist(name.trimmed(), now);
    if (id < 0) {
        QMessageBox::warning(this, "Error", "Could not create playlist.");
        return;
    }
    m_collectionPanel->reloadPlaylists();
    m_collectionPanel->setActivePlaylist(id);
}

void LibraryView::onDeletePlaylistRequested(long long id)
{
    m_db->deletePlaylist(id);
    m_collectionPanel->reloadPlaylists();
    if (m_trackModel->playlistId() == id)
        onCollectionSelected();
}

void LibraryView::onTrackExpanded(const QVariantMap& data)
{
    m_currentSongId = data.value(QStringLiteral("id"), -1LL).toLongLong();
    m_detailPanel->populate(data, m_activePlaylistId);
    m_detailPanel->setVisible(true);
}

void LibraryView::onTrackCollapsed()
{
    m_detailPanel->clear();
}

void LibraryView::onUndoAvailable(bool available)
{
    m_undoBtn->setVisible(available);
}

void LibraryView::onPlayRequested(const QString& filePath,
                                   const QString& title,
                                   const QString& artist)
{
    m_playerBar->setVisible(true);
    m_playerBar->playFile(filePath, title, artist);
    if (m_currentSongId > 0)
        m_db->recordPlay(m_currentSongId);
}

void LibraryView::onScanFinished()
{
    const QVector<Track> newTracks = m_scanWatcher->result();
    if (newTracks.isEmpty()) {
        qInfo() << "[Library] Scan complete: no new files found";
        return;
    }
    qInfo() << "[Library] Scan complete:" << newTracks.size() << "new file(s) found, ingesting...";

    // ingestAndAppend marks each new track with is_analyzing = true
    m_trackModel->ingestAndAppend(newTracks);
    updateStats();

    // Collect newly-added tracks (those with is_analyzing set) for background analysis
    QVector<Track> toAnalyze;
    for (const Track& t : m_trackModel->tracks()) {
        if (t.is_analyzing)
            toAnalyze.append(t);
    }
    if (toAnalyze.isEmpty()) return;

    // Cancel any previous analysis run
    if (m_analyzer) {
        m_analyzer->cancel();
        m_analyzer->deleteLater();
    }

    m_analyzer = new AudioAnalyzer(this);
    connect(m_analyzer, &AudioAnalyzer::trackAnalyzed,
            this, &LibraryView::onTrackAnalyzed);
    connect(m_analyzer, &AudioAnalyzer::finished,
            this, [this]() { onAutoAnalysisFinished(); });

    // Timer forces viewport repaints so the analyzing indicator stays visible
    if (!m_analyzeTimer) {
        m_analyzeTimer = new QTimer(this);
        m_analyzeTimer->setInterval(500);
        connect(m_analyzeTimer, &QTimer::timeout, this, [this]() {
            if (m_trackTable && m_trackTable->viewport())
                m_trackTable->viewport()->update();
        });
    }
    m_analyzeTimer->start();

    qInfo() << "[Library] Auto-analyzing" << toAnalyze.size() << "new tracks in background...";
    m_analyzer->analyzeLibrary(toAnalyze);
}

void LibraryView::onTrackAnalyzed(const Track& updated)
{
    m_trackModel->updateTrackMetadata(updated);
}

void LibraryView::onAutoAnalysisFinished()
{
    if (m_analyzeTimer)
        m_analyzeTimer->stop();

    // Clear any remaining is_analyzing flags (e.g. if analysis was cancelled)
    const QVector<Track>& tracks = m_trackModel->tracks();
    for (int i = 0; i < tracks.size(); ++i) {
        if (tracks[i].is_analyzing)
            m_trackModel->setIsAnalyzing(i, false);
    }

    updateStats();
    qInfo() << "[Library] Background analysis complete.";
}

// Open the export wizard dialog.
void LibraryView::onExportClicked()
{
    ExportWizard wizard(m_db, this);
    wizard.exec();
}

// Run audio analysis on all library tracks.
void LibraryView::onAnalyzeClicked()
{
    const QVector<Track>& tracks = m_trackModel->tracks();
    if (tracks.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("Analyze"),
                                 QStringLiteral("No tracks to analyze."));
        return;
    }

    auto* analyzer = new AudioAnalyzer(this);
    AnalysisProgressDialog dlg(analyzer, this);

    analyzer->analyzeLibrary(tracks);
    if (dlg.exec() == QDialog::Accepted) {
        const QVector<Track> updated = dlg.updatedTracks();
        if (!updated.isEmpty()) {
            m_trackModel->loadFromDatabase(updated);
            updateStats();
            qInfo() << "[Library] Analysis complete:" << updated.size() << "tracks updated";
        }
    }

    analyzer->deleteLater();
}

// Open the export wizard pre-selected to a specific playlist.
void LibraryView::onExportPlaylistRequested(long long playlistId)
{
    ExportWizard wizard(m_db, this);
    wizard.preselectPlaylist(playlistId);
    wizard.exec();
}

void LibraryView::onEditSelectedClicked()
{
    const auto selected = m_trackTable->selectionModel()->selectedRows();
    if (selected.size() < 2) return;

    QVector<Track> tracks;
    tracks.reserve(selected.size());
    for (const QModelIndex& proxyIdx : selected) {
        const QModelIndex srcIdx = m_trackTable->proxy()->mapToSource(proxyIdx);
        if (srcIdx.isValid() && srcIdx.row() < m_trackModel->tracks().size())
            tracks.append(m_trackModel->tracks()[srcIdx.row()]);
    }

    if (tracks.isEmpty()) return;

    auto* dlg = new BatchEditDialog(tracks, m_db, this);
    connect(dlg, &BatchEditDialog::applied, this, [this]() {
        // Reload current view
        if (m_activePlaylistId > 0)
            onPlaylistSelected(m_activePlaylistId);
        else
            onCollectionSelected();
    });
    dlg->exec();
    dlg->deleteLater();
}

void LibraryView::onFindMissingClicked()
{
    auto* dlg = new MissingFilesDialog(m_db, this);
    connect(dlg, &MissingFilesDialog::libraryChanged, this, [this]() {
        if (m_activePlaylistId > 0)
            onPlaylistSelected(m_activePlaylistId);
        else
            onCollectionSelected();
    });
    dlg->exec();
    dlg->deleteLater();
}

void LibraryView::onSelectionChanged()
{
    const auto selected = m_trackTable->selectionModel()->selectedRows();
    const int count = selected.size();
    if (count >= 2) {
        m_editSelectedBtn->setEnabled(true);
        m_editSelectedBtn->setText(QString("EDIT %1 SELECTED").arg(count));
    } else {
        m_editSelectedBtn->setEnabled(false);
        m_editSelectedBtn->setText("EDIT SELECTED");
    }
}

void LibraryView::onPrepareToggleRequested(long long songId, bool currentlyPrepared)
{
    const bool newState = !currentlyPrepared;
    if (!m_db->updateSongPrepared(songId, newState)) {
        qWarning() << "[Library] Failed to update prepared state for song" << songId;
        return;
    }
    const int row = m_trackModel->rowForId(songId);
    if (row >= 0)
        m_trackModel->setPrepared(row, newState);
    qInfo() << "[Library] Song" << songId << (newState ? "marked as prepared" : "unmarked as prepared");
}

void LibraryView::onHistoryDateSelected(const QString& date)
{
    m_activePlaylistId = -1;
    const QVector<Track> tracks = m_db->loadTracksPlayedOn(date);
    m_trackModel->loadFromDatabase(tracks);
    m_detailPanel->clear();
    updateStats();
}

void LibraryView::onExportPlaylistM3uRequested(long long playlistId)
{
    const QVector<Track> tracks = m_db->loadPlaylistSongs(playlistId);
    if (tracks.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("Export M3U"),
                                 QStringLiteral("This playlist has no tracks."));
        return;
    }

    // Find the playlist name for the file suggestion
    QString playlistName;
    const QVector<Playlist> playlists = m_db->loadPlaylists();
    for (const Playlist& p : playlists) {
        if (p.id == playlistId) {
            playlistName = QString::fromStdString(p.name);
            break;
        }
    }

    const QString defaultFile = (playlistName.isEmpty() ? QStringLiteral("playlist") : playlistName)
                                + QStringLiteral(".m3u");
    const QString outputPath = QFileDialog::getSaveFileName(
        this,
        QStringLiteral("Export M3U Playlist"),
        QDir::homePath() + QLatin1Char('/') + defaultFile,
        QStringLiteral("M3U Playlist (*.m3u);;All Files (*)"));
    if (outputPath.isEmpty()) return;

    const bool ok = M3UExporter::exportTracks(tracks, outputPath, playlistName);
    if (!ok) {
        QMessageBox::warning(this, QStringLiteral("Export Failed"),
                             QStringLiteral("Could not write M3U file to:\n") + outputPath);
    } else {
        qInfo() << "[Library] Exported M3U playlist:" << outputPath
                << "(" << tracks.size() << "tracks)";
    }
}

void LibraryView::onDuplicatesClicked()
{
    auto* dlg = new DuplicateDetectorDialog(m_db, this);
    dlg->exec();

    const QVector<long long> removed = dlg->removedIds();
    if (!removed.isEmpty()) {
        // Refresh current view to remove deleted tracks
        if (m_activePlaylistId > 0)
            onPlaylistSelected(m_activePlaylistId);
        else
            onCollectionSelected();
    }
    dlg->deleteLater();
}
