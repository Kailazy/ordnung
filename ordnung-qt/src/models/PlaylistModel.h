#pragma once

#include <QAbstractListModel>
#include <QVector>

#include "core/Playlist.h"

class Database;

class PlaylistModel : public QAbstractListModel
{
    Q_OBJECT
public:
    enum Role {
        PlaylistIdRole   = Qt::UserRole,
        TrackCountRole   = Qt::UserRole + 1,
        FormatCountsRole = Qt::UserRole + 2,  // QVariantMap {format: count}
    };

    explicit PlaylistModel(Database* db, QObject* parent = nullptr);

    void reload();

    int      rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;

    const Playlist* playlistAt(int row) const;

private:
    Database*        m_db;
    QVector<Playlist> m_playlists;
};
