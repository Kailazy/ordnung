#include "TrackModel.h"
#include "services/Database.h"
#include "services/PlaylistImporter.h"
#include "views/table/LibraryTableColumn.h"

#include <QFont>
#include <QColor>
#include <QLocale>
#include <QDateTime>

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

void TrackModel::loadFromDatabase(const QVector<Track>& tracks)
{
    qInfo() << "TrackModel::loadFromDatabase:" << tracks.size() << "tracks";
    beginResetModel();
    m_tracks      = tracks;
    m_playlistId  = -1;
    m_totalCount  = tracks.size();
    m_loadedCount = tracks.size();
    endResetModel();
}

void TrackModel::ingestAndAppend(const QVector<Track>& scanTracks)
{
    const QString now = QDateTime::currentDateTime().toString(Qt::ISODate);
    QVector<Track> toAdd;
    toAdd.reserve(scanTracks.size());
    for (Track t : scanTracks) {
        if (t.match_key.empty())
            t.match_key = PlaylistImporter::makeMatchKey(
                QString::fromStdString(t.artist), QString::fromStdString(t.title)).toStdString();
        if (t.match_key == "|||")
            t.match_key = "file:" + t.filepath;
        if (t.date_added.empty())
            t.date_added = now.toStdString();
        Track dbTrack = m_db->syncFromDisk(t);
        if (dbTrack.id > 0) {
            dbTrack.is_analyzing = true;  // metadata pending background analysis
            toAdd.append(dbTrack);
        }
    }
    if (toAdd.isEmpty()) return;
    const int first = m_tracks.size();
    beginInsertRows({}, first, first + toAdd.size() - 1);
    m_tracks.append(toAdd);
    m_totalCount  += toAdd.size();
    m_loadedCount += toAdd.size();
    endInsertRows();
    qInfo() << "TrackModel::ingestAndAppend:" << toAdd.size() << "new tracks added";
}

void TrackModel::loadFromFiles(const QVector<Track>& tracks)
{
    qDebug() << "TrackModel::loadFromFiles:" << tracks.size() << "tracks from scan";
    QVector<Track> synced;
    synced.reserve(tracks.size());
    const QString now = QDateTime::currentDateTime().toString(Qt::ISODate);
    for (Track t : tracks) {
        if (t.match_key.empty())
            t.match_key = PlaylistImporter::makeMatchKey(
                QString::fromStdString(t.artist), QString::fromStdString(t.title)).toStdString();
        // When artist and title are both empty, makeMatchKey returns "|||" — use filepath
        // so each unnamed file gets its own DB row rather than colliding.
        if (t.match_key == "|||")
            t.match_key = "file:" + t.filepath;
        if (t.date_added.empty())
            t.date_added = now.toStdString();

        // syncFromDisk: if match_key exists in DB, returns the full DB row (user edits intact).
        // If new, inserts and returns the new row. Always sets id > 0 on success.
        const Track dbTrack = m_db->syncFromDisk(t);
        synced.append(dbTrack);
    }
    qInfo() << "TrackModel::loadFromFiles: loaded" << synced.size() << "tracks into model";
    beginResetModel();
    m_tracks      = synced;
    m_playlistId  = -1;
    m_totalCount  = synced.size();
    m_loadedCount = synced.size();
    endResetModel();
}

void TrackModel::searchFts(const QString& query)
{
    const QVector<Track> results = m_db->searchTracks(query);
    loadFromDatabase(results);
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
    return LibraryTableColumn::columnCount();
}

QVariant TrackModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() >= m_tracks.size())
        return {};

    const Track& t = m_tracks.at(index.row());

    switch (role) {
    case Qt::DisplayRole:
    case Qt::EditRole: {
        switch (LibraryTableColumn::columnRole(index.column())) {
        case LibraryTableColumn::Title:   return QString::fromStdString(t.title);
        case LibraryTableColumn::Artist:  return QString::fromStdString(t.artist);
        case LibraryTableColumn::Bpm:     return t.bpm > 0 ? QString::number(static_cast<int>(t.bpm)) : QString();
        case LibraryTableColumn::Key:     return QString::fromStdString(t.key_sig);
        case LibraryTableColumn::Time:    return QString::fromStdString(t.time);
        case LibraryTableColumn::Format:   return QString::fromStdString(t.format.empty() ? "mp3" : t.format);
        }
        return {};
        }

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
        m[QStringLiteral("key")]        = QString::fromStdString(t.key_sig);
        m[QStringLiteral("added")]      = QString::fromStdString(t.date_added);
        m[QStringLiteral("format")]     = QString::fromStdString(t.format);
        m[QStringLiteral("has_aiff")]   = t.has_aiff;
        m[QStringLiteral("filepath")]   = QString::fromStdString(t.filepath);
        return m;
    }

    case HasAiffRole:
        return t.has_aiff;

    case IsAnalyzingRole:
        return t.is_analyzing;

    case ColorLabelRole:
        return t.color_label;

    case PreparedRole:
        return t.is_prepared;

    case Qt::TextAlignmentRole: {
        const auto colRole = LibraryTableColumn::columnRole(index.column());
        switch (colRole) {
        case LibraryTableColumn::Bpm:
        case LibraryTableColumn::Key:
        case LibraryTableColumn::Time:
        case LibraryTableColumn::Format: return Qt::AlignCenter;
        default:                         return QVariant();
        }
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
    case Qt::EditRole: {
        const QString str = value.toString().trimmed();
        switch (LibraryTableColumn::columnRole(index.column())) {
        case LibraryTableColumn::Title:
            t.title = str.toStdString();
            t.match_key = PlaylistImporter::makeMatchKey(
                QString::fromStdString(t.artist), QString::fromStdString(t.title)).toStdString();
            break;
        case LibraryTableColumn::Artist:
            t.artist = str.toStdString();
            t.match_key = PlaylistImporter::makeMatchKey(
                QString::fromStdString(t.artist), QString::fromStdString(t.title)).toStdString();
            break;
        case LibraryTableColumn::Bpm: {
            bool ok = false;
            const double bpm = str.toDouble(&ok);
            t.bpm = (ok && bpm > 0 && bpm < 999) ? bpm : 0.0;
            break;
        }
        case LibraryTableColumn::Key:
            t.key_sig = str.toStdString();
            break;
        case LibraryTableColumn::Time:
            t.time = str.toStdString();
            break;
        case LibraryTableColumn::Format:
            t.format = str.isEmpty() ? "mp3" : str.toLower().toStdString();
            break;
        }
        // All column roles handled above; invalid column falls through and we still update.
        if (t.id > 0)
            m_db->updateSongMetadata(t.id, t);
        emit dataChanged(index, index, {Qt::DisplayRole, Qt::EditRole});
        return true;
    }

    case ExpandedRole:
        t.expanded = value.toBool();
        emit dataChanged(this->index(index.row(), 0),
                         this->index(index.row(), LibraryTableColumn::columnCount() - 1),
                         {ExpandedRole, Qt::DecorationRole});
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
    if (role == Qt::DisplayRole && section >= 0 && section < LibraryTableColumn::columnCount())
        return LibraryTableColumn::headerText(section);

    if (role == Qt::TextAlignmentRole) {
        switch (LibraryTableColumn::columnRole(section)) {
        case LibraryTableColumn::Bpm:
        case LibraryTableColumn::Key:
        case LibraryTableColumn::Time:
        case LibraryTableColumn::Format: return Qt::AlignCenter;
        default:                         return int(Qt::AlignLeft | Qt::AlignVCenter);
        }
    }

    return {};
}

Qt::ItemFlags TrackModel::flags(const QModelIndex& index) const
{
    if (!index.isValid()) return Qt::NoItemFlags;
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable;
}

void TrackModel::setFormat(int row, const QString& format)
{
    if (row < 0 || row >= m_tracks.size()) return;
    m_tracks[row].format = format.toStdString();
    const QModelIndex idx = index(row, LibraryTableColumn::columnIndex(LibraryTableColumn::Format));
    emit dataChanged(idx, idx, {Qt::DisplayRole});
}

void TrackModel::setHasAiff(int row, bool hasAiff)
{
    if (row < 0 || row >= m_tracks.size()) return;
    m_tracks[row].has_aiff = hasAiff;
    emit dataChanged(index(row, 0), index(row, LibraryTableColumn::columnCount() - 1), {HasAiffRole});
}

void TrackModel::setExpanded(int row, bool expanded)
{
    if (row < 0 || row >= m_tracks.size()) return;
    m_tracks[row].expanded = expanded;
    // Emit DecorationRole (not DisplayRole) so QSortFilterProxyModel does not re-sort.
    // The view repaints on any role; delegates read ExpandedRole directly.
    emit dataChanged(index(row, 0), index(row, LibraryTableColumn::columnCount() - 1), {ExpandedRole, Qt::DecorationRole});
}

void TrackModel::setColorLabel(int row, int colorLabel)
{
    if (row < 0 || row >= m_tracks.size()) return;
    Track& t = m_tracks[row];
    t.color_label = colorLabel;
    if (t.id > 0)
        m_db->updateSongColorLabel(t.id, colorLabel);
    const QModelIndex idx = index(row, LibraryTableColumn::columnIndex(LibraryTableColumn::Color));
    emit dataChanged(idx, idx, {ColorLabelRole, Qt::DecorationRole});
}

int TrackModel::rowForId(long long id) const
{
    for (int i = 0; i < m_tracks.size(); ++i)
        if (m_tracks[i].id == id) return i;
    return -1;
}

void TrackModel::setIsAnalyzing(int row, bool analyzing)
{
    if (row < 0 || row >= m_tracks.size()) return;
    m_tracks[row].is_analyzing = analyzing;
    emit dataChanged(index(row, 0), index(row, columnCount() - 1), {IsAnalyzingRole});
}

void TrackModel::setPrepared(int row, bool prepared)
{
    if (row < 0 || row >= m_tracks.size()) return;
    m_tracks[row].is_prepared = prepared;
    const QModelIndex idx = index(row, LibraryTableColumn::columnIndex(LibraryTableColumn::Prepared));
    emit dataChanged(idx, idx, {PreparedRole, Qt::DecorationRole});
}

void TrackModel::updateTrackMetadata(const Track& updated)
{
    const int row = rowForId(updated.id);
    if (row < 0) return;

    Track& t = m_tracks[row];
    // Merge: only overwrite if the new value is more informative
    if (updated.bpm > 0.0)        t.bpm     = updated.bpm;
    if (!updated.key_sig.empty()) t.key_sig = updated.key_sig;
    if (updated.bitrate > 0)      t.bitrate = updated.bitrate;
    if (!updated.time.empty())    t.time    = updated.time;
    t.is_analyzing = false;

    if (t.id > 0)
        m_db->updateSongMetadata(t.id, t);

    emit dataChanged(index(row, 0), index(row, columnCount() - 1),
                     {Qt::DisplayRole, IsAnalyzingRole});
}
