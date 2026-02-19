#pragma once

#include <QUndoCommand>
#include <QPersistentModelIndex>
#include <QString>

class TrackModel;
class Database;

// Undo/redo a single track format change.
// Writes to DB on both redo and undo.
class UpdateFormatCommand : public QUndoCommand
{
public:
    UpdateFormatCommand(TrackModel* model, Database* db,
                        const QModelIndex& index,
                        const QString& newFormat,
                        QUndoCommand* parent = nullptr);

    void redo() override;
    void undo() override;

private:
    TrackModel*           m_model;
    Database*             m_db;
    QPersistentModelIndex m_index;
    long long             m_songId;
    QString               m_oldFormat;
    QString               m_newFormat;
};
