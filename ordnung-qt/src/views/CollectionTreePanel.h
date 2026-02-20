#pragma once

#include <QWidget>
#include <QTreeWidget>

class Database;
class ImportZone;

// CollectionTreePanel — Rekordbox-style hierarchical collection browser.
//
// Replaces the flat PlaylistPanel + FolderPanel popup with a permanent
// QTreeWidget left pane containing:
//   • Collection (all tracks)
//       ├─ All Tracks
//       ├─ Recently Added  (smart: last 30 days)
//       └─ Recently Played (smart: last 7 days)
//   • Playlists           (expandable, drop target for .txt import)
//       ├─ <playlist …>
//       └─ + Create Playlist
//   • Smart Playlists     (built-ins + user-defined)
//   • History             (grouped by session date)
//
// At the bottom sits the drag-and-drop ImportZone from PlaylistPanel.
class CollectionTreePanel : public QWidget
{
    Q_OBJECT
public:
    explicit CollectionTreePanel(Database* db, QWidget* parent = nullptr);

    // Refresh the playlist nodes from the database.
    void reloadPlaylists();

    // Highlight the currently-active playlist node.
    void setActivePlaylist(long long id);

signals:
    // Emitted when the user selects a node. Views respond by loading tracks.
    void collectionSelected();                          // "All Tracks"
    void playlistSelected(long long id);
    void smartPlaylistSelected(const QString& key);    // "needs_aiff", "high_bpm", …
    void historyDateSelected(const QString& date);

    // Playlist management
    void importRequested(const QStringList& filePaths);
    void createPlaylistRequested();
    void deletePlaylistRequested(long long id);
    void exportPlaylistRequested(long long playlistId);

private slots:
    void onItemClicked(QTreeWidgetItem* item, int column);
    void onContextMenu(const QPoint& pos);
    void onImportZoneClicked();
    void onImportZoneFilesDropped(const QStringList& paths);

private:
    void buildTree();
    QTreeWidgetItem* makeCategory(const QString& label);

    Database*       m_db;
    QTreeWidget*    m_tree;
    ImportZone*     m_importZone;

    // Persistent category nodes (never recreated on reload)
    QTreeWidgetItem* m_collectionNode   = nullptr;
    QTreeWidgetItem* m_playlistsNode    = nullptr;
    QTreeWidgetItem* m_smartNode        = nullptr;
    QTreeWidgetItem* m_historyNode      = nullptr;

    long long        m_activePlaylistId = -1;

    // Item data roles
    static constexpr int NodeTypeRole = Qt::UserRole;
    static constexpr int IdRole       = Qt::UserRole + 1;

    enum NodeType {
        AllTracks,
        RecentlyAdded,
        RecentlyPlayed,
        PlaylistNode,
        SmartPlaylist,
        CreatePlaylist,
        HistoryDate,
        CategoryHeader,
    };
};
