#pragma once

#include <QWidget>
#include <QStringList>
#include <QVariantMap>

class PlaylistModel;
class TrackModel;
class Database;
class QUndoStack;
class QLineEdit;
class QComboBox;
class QPushButton;
class QLabel;
class QSplitter;
class QHBoxLayout;
class PlaylistPanel;
class TrackTableView;
class TrackDetailPanel;

// LibraryView — the Library tab content.
// Owns the toolbar (search, bulk format, undo), genre bar, playlist panel,
// track table, and track detail panel.
class LibraryView : public QWidget
{
    Q_OBJECT
public:
    explicit LibraryView(PlaylistModel* playlists, TrackModel* tracks,
                         Database* db, QUndoStack* undoStack,
                         QWidget* parent = nullptr);

    // Load tracks for a playlist and rebuild the genre bar.
    void loadPlaylist(long long playlistId);

    // Reload the playlist sidebar (call after import or delete).
    void reloadPlaylists();

signals:
    void importRequested(const QStringList& paths);
    void deletePlaylistRequested(long long id);

private slots:
    void onPlaylistSelected(long long id);
    void onBulkApply();
    void onTrackExpanded(const QVariantMap& data);
    void onTrackCollapsed();
    void onUndoAvailable(bool available);

private:
    void buildGenreBar(const QStringList& genres);
    void updateStats();

    PlaylistModel*  m_playlistModel;
    TrackModel*     m_trackModel;
    Database*       m_db;
    QUndoStack*     m_undoStack;
    long long       m_activePlaylistId = -1;

    // Toolbar
    QLineEdit*   m_searchEdit  = nullptr;
    QComboBox*   m_formatCombo = nullptr;
    QPushButton* m_applyBtn    = nullptr;
    QPushButton* m_undoBtn     = nullptr;
    QLabel*      m_statsLabel  = nullptr;

    // Genre bar
    QHBoxLayout* m_genreBarLayout = nullptr;
    QString      m_activeGenre;

    // Panels
    PlaylistPanel*    m_playlistPanel = nullptr;
    TrackTableView*   m_trackTable    = nullptr;
    TrackDetailPanel* m_detailPanel   = nullptr;
};
