#pragma once

#include <QAbstractTableModel>
#include <QVector>
#include <QString>

#include "core/Track.h"

class Database;

class TrackModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    // Column count and semantics come from LibraryTableColumn (single source of truth).
    // Custom roles
    enum Role {
        ExpandedRole     = Qt::UserRole,      // bool — detail panel open
        TrackIdRole      = Qt::UserRole + 1,  // long long — song id
        RawTrackRole     = Qt::UserRole + 2,  // QVariant<Track> — full struct
        HasAiffRole      = Qt::UserRole + 3,  // bool
        ColorLabelRole   = Qt::UserRole + 4,  // int — Pioneer color index 0-8
        IsAnalyzingRole  = Qt::UserRole + 5,  // bool — background ffprobe in progress
    };

    explicit TrackModel(Database* db, QObject* parent = nullptr);

    // Load tracks for a playlist. Resets the model.
    void loadPlaylist(long long playlistId);

    // Populate the model from already-fetched DB rows (no DB sync, no scan).
    // Use on startup to show the library instantly.
    void loadFromDatabase(const QVector<Track>& tracks);

    // Ingest new scan results: runs syncFromDisk for each track and appends
    // the resulting rows to the existing model without resetting it.
    void ingestAndAppend(const QVector<Track>& scanTracks);

    // Load tracks directly from a pre-scanned list (filesystem-based library).
    void loadFromFiles(const QVector<Track>& tracks);

    // Replace current track list with FTS5 search results.
    void searchFts(const QString& query);

    // Clear all tracks.
    void clear();

    // QAbstractTableModel interface
    int      rowCount(const QModelIndex& parent = {}) const override;
    int      columnCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    bool     setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole) override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;

    // Lazy loading
    bool canFetchMore(const QModelIndex& parent) const override;
    void fetchMore(const QModelIndex& parent) override;

    // Access all currently loaded tracks (for bulk operations).
    const QVector<Track>& tracks() const { return m_tracks; }

    // Direct mutations (called by commands).
    void setFormat(int row, const QString& format);
    void setHasAiff(int row, bool hasAiff);
    void setExpanded(int row, bool expanded);
    void setColorLabel(int row, int colorLabel);
    void setIsAnalyzing(int row, bool analyzing);

    // Update bpm/key/bitrate/time for a track after background analysis completes.
    // Clears is_analyzing, persists to DB, and emits dataChanged.
    void updateTrackMetadata(const Track& updated);

    // Find row by song id.
    int rowForId(long long id) const;

    long long playlistId() const { return m_playlistId; }

private:
    Database*     m_db;
    QVector<Track> m_tracks;
    long long     m_playlistId  = -1;
    int           m_totalCount  = 0;
    int           m_loadedCount = 0;

    static constexpr int kBatchSize = 200;
};
