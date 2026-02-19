#include "BulkFormatCommand.h"
#include "models/TrackModel.h"
#include "services/Database.h"

BulkFormatCommand::BulkFormatCommand(TrackModel* model, Database* db,
                                     const QVector<FormatSnapshot>& snapshot,
                                     const QString& newFormat,
                                     QUndoCommand* parent)
    : QUndoCommand(QObject::tr("Set all to %1").arg(newFormat), parent)
    , m_model(model)
    , m_db(db)
    , m_snapshot(snapshot)
    , m_newFormat(newFormat)
{}

void BulkFormatCommand::redo()
{
    QVector<long long> ids;
    ids.reserve(m_snapshot.size());
    for (const FormatSnapshot& s : m_snapshot) {
        m_model->setFormat(s.row, m_newFormat);
        ids.append(s.songId);
    }
    m_db->bulkUpdateFormat(m_newFormat, ids);
}

void BulkFormatCommand::undo()
{
    // Each song may have had a different format — restore individually.
    // Group by old format to minimise DB roundtrips.
    QMap<QString, QVector<long long>> byFormat;
    for (const FormatSnapshot& s : m_snapshot) {
        m_model->setFormat(s.row, s.oldFormat);
        byFormat[s.oldFormat].append(s.songId);
    }
    for (auto it = byFormat.begin(); it != byFormat.end(); ++it)
        m_db->bulkUpdateFormat(it.key(), it.value());
}
