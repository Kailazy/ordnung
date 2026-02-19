#pragma once

#include <QUndoCommand>
#include <QString>
#include <QVector>

class TrackModel;
class Database;

struct FormatSnapshot {
    int       row;
    long long songId;
    QString   oldFormat;
};

// Undo/redo bulk format change across all currently visible (proxy-filtered) rows.
class BulkFormatCommand : public QUndoCommand
{
public:
    // snapshot: the current {row, songId, format} for every row being changed.
    // newFormat: format to apply.
    BulkFormatCommand(TrackModel* model, Database* db,
                      const QVector<FormatSnapshot>& snapshot,
                      const QString& newFormat,
                      QUndoCommand* parent = nullptr);

    void redo() override;
    void undo() override;

private:
    TrackModel*             m_model;
    Database*               m_db;
    QVector<FormatSnapshot> m_snapshot;
    QString                 m_newFormat;
};
