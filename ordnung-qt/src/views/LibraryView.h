#pragma once

#include <QWidget>
#include <QVariantMap>
#include <QFutureWatcher>
#include <QTimer>
#include "core/Track.h"

class TrackModel;
class Database;
class QUndoStack;
class QLineEdit;
class QPushButton;
class QLabel;
class QAction;
class CollectionTreePanel;
class TrackTableView;
class TrackDetailPanel;
class PlayerBar;
class ExportWizard;
class AudioAnalyzer;
class AnalysisProgressDialog;
class BatchEditDialog;
class MissingFilesDialog;

// LibraryView — the Library tab content.
// Owns the toolbar (search, undo, folder button), the CollectionTreePanel
// left pane, track table, track detail panel, and audio player bar.
class LibraryView : public QWidget
{
    Q_OBJECT
public:
    explicit LibraryView(TrackModel* tracks, Database* db,
                         QUndoStack* undoStack, QWidget* parent = nullptr);

    // Set and scan the library folder (call on startup restore).
    void setLibraryFolder(const QString& path);

signals:
    void libraryFolderChanged(const QString& path);

private slots:
    void onBrowseFolderClicked();
    void onCollectionSelected();
    void onPlaylistSelected(long long id);
    void onSmartPlaylistSelected(const QString& key);
    void onImportRequested(const QStringList& filePaths);
    void onCreatePlaylistRequested();
    void onDeletePlaylistRequested(long long id);
    void onTrackExpanded(const QVariantMap& data);
    void onTrackCollapsed();
    void onUndoAvailable(bool available);
    void onPlayRequested(const QString& filePath,
                         const QString& title,
                         const QString& artist);
    void onScanFinished();
    void onTrackAnalyzed(const Track& updated);
    void onAutoAnalysisFinished();
    void onExportClicked();
    void onAnalyzeClicked();
    void onExportPlaylistRequested(long long playlistId);
    void onEditSelectedClicked();
    void onFindMissingClicked();
    void onSelectionChanged();

private:
    void loadAndScan();
    void rescan();
    void importPlaylistFile(const QString& filePath);
    void updateStats();

    TrackModel*  m_trackModel;
    Database*    m_db;
    QUndoStack*  m_undoStack;
    QString      m_libraryFolder;

    // Toolbar
    QPushButton* m_folderBtn   = nullptr;  // shows folder name, click to browse
    QLineEdit*   m_searchEdit  = nullptr;
    QPushButton* m_undoBtn     = nullptr;
    QPushButton* m_exportBtn   = nullptr;
    QPushButton* m_analyzeBtn      = nullptr;
    QPushButton* m_editSelectedBtn = nullptr;
    QPushButton* m_missingBtn      = nullptr;
    QLabel*      m_searchBadge     = nullptr;
    QLabel*      m_statsLabel      = nullptr;

    // Panels
    CollectionTreePanel* m_collectionPanel = nullptr;
    TrackTableView*      m_trackTable      = nullptr;
    TrackDetailPanel*    m_detailPanel     = nullptr;
    PlayerBar*           m_playerBar       = nullptr;

    // Async scan
    QFutureWatcher<QVector<Track>>* m_scanWatcher = nullptr;

    // Background metadata analysis (auto-started after fast scan)
    AudioAnalyzer* m_analyzer      = nullptr;
    QTimer*        m_analyzeTimer  = nullptr;  // forces viewport repaints while analyzing

    long long m_activePlaylistId = -1;
};
