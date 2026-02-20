#pragma once

#include <QObject>
#include <QSqlDatabase>
#include <QString>
#include <QVector>
#include <QMap>

#include "core/Track.h"
#include "core/Playlist.h"
#include "core/ConversionJob.h"
#include "core/CuePoint.h"

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

    // Load all library songs (scanned from disk) whose filepath starts with folderPrefix.
    // Use this on startup to populate the library from DB without rescanning.
    QVector<Track> loadLibrarySongs(const QString& folderPrefix);

    // Sync a scan-derived track with the DB. If a row with the same match_key
    // already exists, returns that row with all user-edited fields intact (only
    // the filepath is taken from scanTrack). If no row exists, inserts it.
    // Always returns a Track with id > 0 on success, id == -1 on DB error.
    Track syncFromDisk(const Track& scanTrack);

    // Load a full song row by its primary key. Returns a default-constructed Track on failure.
    Track loadSongById(long long id);

    // Link a song to a playlist.
    bool linkSongToPlaylist(long long songId, long long playlistId);

    // Format mutations.
    bool updateSongFormat(long long songId, const QString& format);
    bool bulkUpdateFormat(const QString& format, const QVector<long long>& songIds);
    bool updateSongAiff(long long songId, bool hasAiff);

    // Color label (0 = none, 1-8 = Pioneer palette index).
    bool updateSongColorLabel(long long songId, int colorLabel);

    // Update full song metadata (title, artist, album, genre, bpm, time, key_sig, format).
    bool updateSongMetadata(long long songId, const Track& t);

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

    // Library folder (filesystem-based library scan path).
    QString loadLibraryFolder();
    bool    saveLibraryFolder(const QString& folder);

    // ── Cue Points ─────────────────────────────────────────────────────────────
    QVector<CuePoint> loadCuePoints(long long songId);
    bool insertCuePoint(CuePoint& cue);          // sets cue.id on success
    bool updateCuePoint(const CuePoint& cue);
    bool deleteCuePoint(long long cueId);
    bool deleteAllCuePoints(long long songId);

    // ── FTS5 Search ─────────────────────────────────────────────────────────
    // Full-text search across title/artist/album/genre/comment using FTS5 BM25 ranking.
    // Returns matching tracks ordered by relevance. Empty query returns all tracks.
    QVector<Track> searchTracks(const QString& query);

    // ── Missing File Relocator ──────────────────────────────────────────────
    // Returns all tracks whose filepath does not exist on disk.
    QVector<Track> findMissingTracks();

    // Update the filepath for a track (after user relocates the file).
    bool updateTrackFilepath(long long songId, const QString& newPath);

    // Remove a track from songs and playlist_songs.
    bool deleteTrack(long long songId);

    // ── Export helpers ─────────────────────────────────────────────────────────
    // Load every song row in the database (no folder/playlist filter).
    QVector<Track> loadAllSongs();
    // Load all songs belonging to a specific playlist.
    QVector<Track> loadPlaylistSongs(long long playlistId);
    // Persist analysis results (BPM, key, bitrate, duration) for a song.
    bool updateSongAnalysis(long long songId, double bpm, const QString& key,
                            int bitrate, const QString& duration);

    // Persist Essentia deep analysis results for a song.
    bool updateSongEssentiaAnalysis(long long songId, const QString& moodTags,
                                     const QString& styleTags, float danceability,
                                     float valence, float vocalProb);

    // ── Waveform Cache ─────────────────────────────────────────────────────────
    QByteArray loadWaveformOverview(long long songId);
    bool       saveWaveformOverview(long long songId, const QByteArray& peaks);

    // ── Utility ────────────────────────────────────────────────────────────────
    // Check whether a filepath is already tracked in downloads.
    bool downloadExists(const QString& filepath);

private:
    void runMigrations();
    QSqlDatabase m_db;
    QString      m_error;
    QString      m_connectionName;
};
