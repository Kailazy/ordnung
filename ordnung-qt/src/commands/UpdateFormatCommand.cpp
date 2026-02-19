#include "UpdateFormatCommand.h"
#include "models/TrackModel.h"
#include "services/Database.h"

UpdateFormatCommand::UpdateFormatCommand(TrackModel* model, Database* db,
                                         const QModelIndex& index,
                                         const QString& newFormat,
                                         QUndoCommand* parent)
    : QUndoCommand(QObject::tr("Change format to %1").arg(newFormat), parent)
    , m_model(model)
    , m_db(db)
    , m_index(index)
    , m_newFormat(newFormat)
{
    // Capture the current format as the "old" value before any change.
    m_oldFormat = index.data(Qt::DisplayRole).toString();
    m_songId    = index.data(TrackModel::TrackIdRole).toLongLong();
}

void UpdateFormatCommand::redo()
{
    if (!m_index.isValid()) return;
    const int row = m_index.row();
    m_model->setFormat(row, m_newFormat);
    m_db->updateSongFormat(m_songId, m_newFormat);
}

void UpdateFormatCommand::undo()
{
    if (!m_index.isValid()) return;
    const int row = m_index.row();
    m_model->setFormat(row, m_oldFormat);
    m_db->updateSongFormat(m_songId, m_oldFormat);
}
