#include "PlaylistModel.h"
#include "services/Database.h"

#include <QVariantMap>

PlaylistModel::PlaylistModel(Database* db, QObject* parent)
    : QAbstractListModel(parent)
    , m_db(db)
{}

void PlaylistModel::reload()
{
    beginResetModel();
    m_playlists = m_db->loadPlaylists();
    endResetModel();
}

int PlaylistModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid()) return 0;
    return m_playlists.size();
}

QVariant PlaylistModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() >= m_playlists.size())
        return {};

    const Playlist& p = m_playlists.at(index.row());

    switch (role) {
    case Qt::DisplayRole:
        return QString::fromStdString(p.name);

    case PlaylistIdRole:
        return static_cast<qlonglong>(p.id);

    case TrackCountRole:
        return p.total;

    case FormatCountsRole: {
        QVariantMap m;
        for (const auto& [fmt, cnt] : p.format_counts)
            m[QString::fromStdString(fmt)] = cnt;
        return m;
    }

    default:
        return {};
    }
}

Qt::ItemFlags PlaylistModel::flags(const QModelIndex& index) const
{
    if (!index.isValid()) return Qt::NoItemFlags;
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

const Playlist* PlaylistModel::playlistAt(int row) const
{
    if (row < 0 || row >= m_playlists.size()) return nullptr;
    return &m_playlists.at(row);
}
