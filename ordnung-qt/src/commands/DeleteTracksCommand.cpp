#include "DeleteTracksCommand.h"
#include "models/PlaylistModel.h"
#include "services/Database.h"

DeleteTracksCommand::DeleteTracksCommand(PlaylistModel* model, Database* db,
                                         long long playlistId,
                                         const QString& name,
                                         const QString& importedAt,
                                         const QVector<long long>& songIds,
                                         QUndoCommand* parent)
    : QUndoCommand(QObject::tr("Delete playlist \"%1\"").arg(name), parent)
    , m_model(model)
    , m_db(db)
    , m_playlistId(playlistId)
    , m_name(name)
    , m_importedAt(importedAt)
    , m_songIds(songIds)
{}

void DeleteTracksCommand::redo()
{
    m_db->deletePlaylist(m_restoredId > 0 ? m_restoredId : m_playlistId);
    m_model->reload();
}

void DeleteTracksCommand::undo()
{
    // Re-create the playlist
    m_restoredId = m_db->insertPlaylist(m_name, m_importedAt);
    if (m_restoredId < 0) return;

    // Re-link all songs
    for (long long songId : m_songIds)
        m_db->linkSongToPlaylist(songId, m_restoredId);

    m_model->reload();
}
