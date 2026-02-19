#pragma once

#include <QMainWindow>

#include "services/Database.h"   // WatchConfig

class ConversionWorker;
class FolderWatcher;
class PlaylistImporter;
class PlaylistModel;
class TrackModel;
class DownloadsModel;
class QUndoStack;
class QThread;
class QStackedWidget;
class QPushButton;
class LibraryView;
class DownloadsView;

// MainWindow — owns all services, models, and the top-level window layout.
// Wires every signal/slot connection between the app layers.
class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(const QString& themeSheet, QWidget* parent = nullptr);
    ~MainWindow() override;

private slots:
    void onImportRequested(const QStringList& paths);
    void onDeletePlaylist(long long id);
    void onSaveConfig(const WatchConfig& cfg);
    void onScanRequested(const QString& folder);
    void onConvertAll(const QString& outputFolder);
    void onConvertSingle(long long downloadId, const QString& sourcePath,
                         const QString& outputFolder);
    void onDeleteDownload(long long id);
    void onConversionStarted(long long convId, long long downloadId);
    void onConversionFinished(long long convId, long long downloadId,
                              bool success, const QString& error);
    void onWorkerLog(const QString& line);
    void switchToLibrary();
    void switchToDownloads();

private:
    void setupServices();
    void buildSidebar(QWidget* container);
    void restoreState();
    void setNavActive(QPushButton* active);

    // Services
    Database*         m_db          = nullptr;
    ConversionWorker* m_converter   = nullptr;
    FolderWatcher*    m_watcher     = nullptr;
    PlaylistImporter* m_importer    = nullptr;
    QThread*          m_workerThread = nullptr;

    // Models
    PlaylistModel*  m_playlists = nullptr;
    TrackModel*     m_tracks    = nullptr;
    DownloadsModel* m_downloads = nullptr;
    QUndoStack*     m_undoStack = nullptr;

    // UI
    QPushButton*    m_libNavBtn      = nullptr;
    QPushButton*    m_dlNavBtn       = nullptr;
    QStackedWidget* m_stack          = nullptr;
    LibraryView*    m_libraryView    = nullptr;
    DownloadsView*  m_downloadsView  = nullptr;
};
