#pragma once

#include <QUndoCommand>
#include <QVector>

class PlaylistModel;
class Database;

// Undo/redo deletion of a playlist.
// "Delete playlist" means removing the playlist record; songs are kept in the
// songs table (they may belong to other playlists). The playlist_songs rows are
// deleted via CASCADE in the DB schema.
//
// Undo re-creates the playlist record with the same name and re-links every
// song_id. This requires capturing the playlist_songs rows before deletion.
class DeleteTracksCommand : public QUndoCommand
{
public:
    // playlistId: the playlist being deleted.
    // name: the playlist name (needed for re-creation on undo).
    // importedAt: timestamp string (needed for re-creation).
    // songIds: all songs that were in this playlist (captured before deletion).
    DeleteTracksCommand(PlaylistModel* model, Database* db,
                        long long playlistId,
                        const QString& name,
                        const QString& importedAt,
                        const QVector<long long>& songIds,
                        QUndoCommand* parent = nullptr);

    void redo() override;
    void undo() override;

private:
    PlaylistModel*     m_model;
    Database*          m_db;
    long long          m_playlistId;
    QString            m_name;
    QString            m_importedAt;
    QVector<long long> m_songIds;
    long long          m_restoredId = -1;  // set after undo re-inserts the playlist
};
