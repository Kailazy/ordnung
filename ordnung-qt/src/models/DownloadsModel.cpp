#include "DownloadsModel.h"
#include "services/Database.h"

DownloadsModel::DownloadsModel(Database* db, QObject* parent)
    : QAbstractTableModel(parent)
    , m_db(db)
{}

void DownloadsModel::reload()
{
    beginResetModel();
    m_downloads = m_db->loadDownloads();
    endResetModel();
}

void DownloadsModel::setConversionStatus(long long downloadId, long long convId,
                                         ConversionStatus status, const QString& errorMsg)
{
    for (int i = 0; i < m_downloads.size(); ++i) {
        if (m_downloads[i].id == downloadId) {
            m_downloads[i].has_conversion = true;
            m_downloads[i].conv_id        = convId;
            m_downloads[i].conv_status    = status;
            m_downloads[i].conv_error     = errorMsg.toStdString();

            const QModelIndex left  = index(i, ColStatus);
            const QModelIndex right = index(i, ColAction);
            emit dataChanged(left, right, {Qt::DisplayRole, ConvStatusRole});
            return;
        }
    }
}

void DownloadsModel::removeRow(long long downloadId)
{
    for (int i = 0; i < m_downloads.size(); ++i) {
        if (m_downloads[i].id == downloadId) {
            beginRemoveRows({}, i, i);
            m_downloads.removeAt(i);
            endRemoveRows();
            return;
        }
    }
}

int DownloadsModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid()) return 0;
    return m_downloads.size();
}

int DownloadsModel::columnCount(const QModelIndex& parent) const
{
    if (parent.isValid()) return 0;
    return ColCount;
}

QVariant DownloadsModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() >= m_downloads.size())
        return {};

    const Download& d = m_downloads.at(index.row());

    switch (role) {
    case Qt::DisplayRole:
        switch (index.column()) {
        case ColFilename: return QString::fromStdString(d.filename);
        case ColExt:      return QString::fromStdString(d.extension).toUpper();
        case ColSize:     return QString::number(d.size_mb, 'f', 1) + QStringLiteral(" MB");
        case ColStatus:   return statusLabel(d.has_conversion ? d.conv_status : ConversionStatus::None);
        case ColAction: {
            if (!d.has_conversion) return QStringLiteral("convert");
            if (d.conv_status == ConversionStatus::Failed) return QStringLiteral("retry");
            return QString();
        }
        default: return {};
        }

    case Qt::TextAlignmentRole:
        switch (index.column()) {
        case ColExt:
        case ColSize:
        case ColStatus:
        case ColAction:  return Qt::AlignCenter;
        default:         return QVariant();
        }

    case DownloadIdRole:
        return static_cast<qlonglong>(d.id);

    case ConvIdRole:
        return static_cast<qlonglong>(d.conv_id);

    case ConvStatusRole:
        return static_cast<int>(d.has_conversion ? d.conv_status : ConversionStatus::None);

    case FilePathRole:
        return QString::fromStdString(d.filepath);

    case Qt::ToolTipRole:
        if (index.column() == ColFilename)
            return QString::fromStdString(d.filepath);
        if (index.column() == ColStatus && d.conv_status == ConversionStatus::Failed)
            return QString::fromStdString(d.conv_error);
        return {};

    default:
        return {};
    }
}

QVariant DownloadsModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole) return {};
    switch (section) {
    case ColFilename: return QStringLiteral("FILE");
    case ColExt:      return QStringLiteral("EXT");
    case ColSize:     return QStringLiteral("SIZE");
    case ColStatus:   return QStringLiteral("STATUS");
    case ColAction:   return QString();
    default:          return {};
    }
}

Qt::ItemFlags DownloadsModel::flags(const QModelIndex& index) const
{
    if (!index.isValid()) return Qt::NoItemFlags;
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

const Download* DownloadsModel::downloadAt(int row) const
{
    if (row < 0 || row >= m_downloads.size()) return nullptr;
    return &m_downloads.at(row);
}

QString DownloadsModel::statusLabel(ConversionStatus s)
{
    switch (s) {
    case ConversionStatus::Pending:    return QStringLiteral("pending");
    case ConversionStatus::Converting: return QStringLiteral("converting");
    case ConversionStatus::Done:       return QStringLiteral("done");
    case ConversionStatus::Failed:     return QStringLiteral("failed");
    default:                           return QString();
    }
}
