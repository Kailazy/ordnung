#include "TrackModel.h"
#include "services/Database.h"

#include <QFont>
#include <QColor>
#include <QLocale>

TrackModel::TrackModel(Database* db, QObject* parent)
    : QAbstractTableModel(parent)
    , m_db(db)
{}

void TrackModel::loadPlaylist(long long playlistId)
{
    beginResetModel();
    m_tracks.clear();
    m_playlistId  = playlistId;
    m_totalCount  = m_db->countTracks(playlistId);
    m_loadedCount = 0;
    endResetModel();

    // Immediately fetch the first batch so the view is not empty on load.
    fetchMore({});
}

void TrackModel::clear()
{
    beginResetModel();
    m_tracks.clear();
    m_playlistId  = -1;
    m_totalCount  = 0;
    m_loadedCount = 0;
    endResetModel();
}

bool TrackModel::canFetchMore(const QModelIndex& /*parent*/) const
{
    return m_loadedCount < m_totalCount;
}

void TrackModel::fetchMore(const QModelIndex& /*parent*/)
{
    if (m_playlistId < 0) return;

    const int remaining = m_totalCount - m_loadedCount;
    const int batch     = qMin(kBatchSize, remaining);
    if (batch <= 0) return;

    const QVector<Track> newTracks = m_db->loadTracks(m_playlistId, m_loadedCount, batch);
    if (newTracks.isEmpty()) return;

    beginInsertRows({}, m_loadedCount, m_loadedCount + newTracks.size() - 1);
    m_tracks.append(newTracks);
    m_loadedCount += newTracks.size();
    endInsertRows();
}

int TrackModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid()) return 0;
    return m_tracks.size();
}

int TrackModel::columnCount(const QModelIndex& parent) const
{
    if (parent.isValid()) return 0;
    return ColCount;
}

QVariant TrackModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() >= m_tracks.size())
        return {};

    const Track& t = m_tracks.at(index.row());

    switch (role) {
    case Qt::DisplayRole:
        switch (index.column()) {
        case ColChevron: return t.expanded ? QStringLiteral("▾") : QStringLiteral("▸");
        case ColTitle:   return QString::fromStdString(t.title);
        case ColBpm:     return t.bpm > 0 ? QString::number(static_cast<int>(t.bpm)) : QString();
        case ColKey:     return QString::fromStdString(t.key_sig);
        case ColTime:    return QString::fromStdString(t.time);
        case ColFormat:  return QString::fromStdString(t.format.empty() ? "mp3" : t.format);
        default:         return {};
        }

    case Qt::UserRole + 4:  // ArtistRole for title cell delegate
        if (index.column() == ColTitle)
            return QString::fromStdString(t.artist);
        return {};

    case ExpandedRole:
        return t.expanded;

    case TrackIdRole:
        return static_cast<qlonglong>(t.id);

    case RawTrackRole: {
        // Pack the whole track into a QVariant for the detail panel.
        // We use a QVariantMap since Track is not registered as QMetaType.
        QVariantMap m;
        m[QStringLiteral("id")]         = static_cast<qlonglong>(t.id);
        m[QStringLiteral("title")]      = QString::fromStdString(t.title);
        m[QStringLiteral("artist")]     = QString::fromStdString(t.artist);
        m[QStringLiteral("album")]      = QString::fromStdString(t.album);
        m[QStringLiteral("genre")]      = QString::fromStdString(t.genre);
        m[QStringLiteral("bpm")]        = t.bpm;
        m[QStringLiteral("rating")]     = t.rating;
        m[QStringLiteral("time")]       = QString::fromStdString(t.time);
        m[QStringLiteral("key_sig")]    = QString::fromStdString(t.key_sig);
        m[QStringLiteral("date_added")] = QString::fromStdString(t.date_added);
        m[QStringLiteral("format")]     = QString::fromStdString(t.format);
        m[QStringLiteral("has_aiff")]   = t.has_aiff;
        return m;
    }

    case HasAiffRole:
        return t.has_aiff;

    case Qt::TextAlignmentRole:
        switch (index.column()) {
        case ColChevron: return Qt::AlignCenter;
        case ColBpm:
        case ColKey:
        case ColTime:    return Qt::AlignCenter;
        case ColFormat:  return Qt::AlignCenter;
        default:         return QVariant();
        }

    case Qt::ForegroundRole:
        switch (index.column()) {
        case ColChevron: return QColor(t.expanded ? 0x4fc3f7 : 0x444444);
        default:         return {};
        }

    default:
        return {};
    }
}

bool TrackModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if (!index.isValid() || index.row() >= m_tracks.size())
        return false;

    Track& t = m_tracks[index.row()];

    switch (role) {
    case Qt::EditRole:
        if (index.column() == ColFormat) {
            t.format = value.toString().toStdString();
            emit dataChanged(index, index, {Qt::DisplayRole});
            return true;
        }
        break;

    case ExpandedRole:
        t.expanded = value.toBool();
        emit dataChanged(this->index(index.row(), 0),
                         this->index(index.row(), ColCount - 1),
                         {ExpandedRole, Qt::DisplayRole});
        return true;

    case HasAiffRole:
        t.has_aiff = value.toBool();
        emit dataChanged(index, index, {HasAiffRole});
        return true;

    default:
        break;
    }
    return false;
}

QVariant TrackModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal) return {};

    if (role == Qt::DisplayRole) {
        switch (section) {
        case ColChevron: return QString();
        case ColTitle:   return QStringLiteral("TRACK");
        case ColBpm:     return QStringLiteral("BPM");
        case ColKey:     return QStringLiteral("KEY");
        case ColTime:    return QStringLiteral("TIME");
        case ColFormat:  return QStringLiteral("FORMAT");
        default:         return {};
        }
    }

    if (role == Qt::TextAlignmentRole) {
        switch (section) {
        case ColBpm:
        case ColKey:
        case ColTime:
        case ColFormat: return Qt::AlignCenter;
        default:        return Qt::AlignLeft;
        }
    }

    return {};
}

Qt::ItemFlags TrackModel::flags(const QModelIndex& index) const
{
    if (!index.isValid()) return Qt::NoItemFlags;
    Qt::ItemFlags f = Qt::ItemIsEnabled | Qt::ItemIsSelectable;
    if (index.column() == ColFormat)
        f |= Qt::ItemIsEditable;
    return f;
}

void TrackModel::setFormat(int row, const QString& format)
{
    if (row < 0 || row >= m_tracks.size()) return;
    m_tracks[row].format = format.toStdString();
    const QModelIndex idx = index(row, ColFormat);
    emit dataChanged(idx, idx, {Qt::DisplayRole});
}

void TrackModel::setHasAiff(int row, bool hasAiff)
{
    if (row < 0 || row >= m_tracks.size()) return;
    m_tracks[row].has_aiff = hasAiff;
    emit dataChanged(index(row, 0), index(row, ColCount - 1), {HasAiffRole});
}

void TrackModel::setExpanded(int row, bool expanded)
{
    if (row < 0 || row >= m_tracks.size()) return;
    m_tracks[row].expanded = expanded;
    emit dataChanged(index(row, 0), index(row, ColCount - 1), {ExpandedRole, Qt::DisplayRole});
}

int TrackModel::rowForId(long long id) const
{
    for (int i = 0; i < m_tracks.size(); ++i)
        if (m_tracks[i].id == id) return i;
    return -1;
}
