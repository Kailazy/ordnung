#pragma once

#include <QObject>
#include <QSqlDatabase>
#include <QString>
#include <QVector>
#include <QMap>

#include "core/Track.h"
#include "core/Playlist.h"
#include "core/ConversionJob.h"

// WatchConfig mirrors the config table rows we care about.
struct WatchConfig {
    QString watchFolder;
    QString outputFolder;
    bool    autoConvert   = true;
};

// PlaylistMembership: used by TrackDetailPanel to show/toggle playlist chips.
struct PlaylistMembership {
    long long   id;
    QString     name;
    bool        member;
};

class Database : public QObject
{
    Q_OBJECT
public:
    explicit Database(QObject* parent = nullptr);
    ~Database() override;

    // Opens (or creates) the DB at the standard app data path.
    // Returns false on failure; errorString() populated.
    bool open();
    QString errorString() const { return m_error; }

    // ── Playlists ──────────────────────────────────────────────────────────────
    QVector<Playlist> loadPlaylists();
    // Returns the new playlist id, or -1 on failure.
    long long insertPlaylist(const QString& name, const QString& importedAt);
    bool deletePlaylist(long long id);

    // ── Songs ──────────────────────────────────────────────────────────────────
    // Load all tracks for a playlist (paginated for canFetchMore support).
    QVector<Track> loadTracks(long long playlistId, int offset, int limit);
    int            countTracks(long long playlistId);

    // Dedup-aware insert: returns song_id (existing or new), -1 on error.
    long long upsertSong(const Track& t);

    // Link a song to a playlist.
    bool linkSongToPlaylist(long long songId, long long playlistId);

    // Format mutations.
    bool updateSongFormat(long long songId, const QString& format);
    bool bulkUpdateFormat(const QString& format, const QVector<long long>& songIds);
    bool updateSongAiff(long long songId, bool hasAiff);

    // Playlist membership for a song.
    QVector<PlaylistMembership> getSongPlaylists(long long songId);
    bool addSongToPlaylist(long long songId, long long playlistId);
    bool removeSongFromPlaylist(long long songId, long long playlistId);

    // ── Downloads ──────────────────────────────────────────────────────────────
    QVector<Download> loadDownloads();
    // Insert a download record if not already present. Returns id or -1.
    long long insertDownload(const QString& filename, const QString& filepath,
                             const QString& extension, double sizeMb,
                             const QString& detectedAt);
    bool deleteDownload(long long id);

    // ── Conversions ────────────────────────────────────────────────────────────
    long long insertConversion(long long downloadId, const QString& sourcePath,
                               const QString& outputPath, const QString& sourceExt,
                               double sizeMb, const QString& startedAt);
    bool updateConversionStatus(long long convId, const QString& status,
                                const QString& errorMsg = QString(),
                                const QString& finishedAt = QString());

    // ── Config ─────────────────────────────────────────────────────────────────
    WatchConfig loadWatchConfig();
    bool saveWatchConfig(const WatchConfig& cfg);

    // ── Utility ────────────────────────────────────────────────────────────────
    // Check whether a filepath is already tracked in downloads.
    bool downloadExists(const QString& filepath);

private:
    void runMigrations();
    QSqlDatabase m_db;
    QString      m_error;
    QString      m_connectionName;
};
