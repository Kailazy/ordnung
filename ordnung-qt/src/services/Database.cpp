#include "Database.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QSqlRecord>
#include <QStandardPaths>
#include <QDir>
#include <QDateTime>
#include <QDebug>
#include <QFileInfo>
#include <QRegularExpression>
#include <QUuid>

static QString convStatusToString(ConversionStatus s)
{
    switch (s) {
    case ConversionStatus::Pending:    return QStringLiteral("pending");
    case ConversionStatus::Converting: return QStringLiteral("converting");
    case ConversionStatus::Done:       return QStringLiteral("done");
    case ConversionStatus::Failed:     return QStringLiteral("failed");
    default:                           return QStringLiteral("none");
    }
}

static ConversionStatus convStatusFromString(const QString& s)
{
    if (s == QLatin1String("converting")) return ConversionStatus::Converting;
    if (s == QLatin1String("done"))       return ConversionStatus::Done;
    if (s == QLatin1String("failed"))     return ConversionStatus::Failed;
    if (s == QLatin1String("pending"))    return ConversionStatus::Pending;
    return ConversionStatus::None;
}

// ─────────────────────────────────────────────────────────────────────────────

Database::Database(QObject* parent)
    : QObject(parent)
    , m_connectionName(QUuid::createUuid().toString(QUuid::WithoutBraces))
{}

Database::~Database()
{
    if (m_db.isOpen())
        m_db.close();
    QSqlDatabase::removeDatabase(m_connectionName);
}

bool Database::open()
{
    // Determine path
    const QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (!QDir().mkpath(dataDir)) {
        m_error = QStringLiteral("Cannot create app data directory: ") + dataDir;
        return false;
    }
    const QString dbPath = dataDir + QStringLiteral("/eyebags.db");

    m_db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connectionName);
    m_db.setDatabaseName(dbPath);

    if (!m_db.open()) {
        m_error = m_db.lastError().text();
        qCritical() << "Database::open failed:" << m_error;
        return false;
    }
    qInfo() << "Database opened:" << dbPath;

    // Enable WAL and foreign keys
    QSqlQuery q(m_db);
    q.exec(QStringLiteral("PRAGMA journal_mode=WAL"));
    q.exec(QStringLiteral("PRAGMA foreign_keys=ON"));

    runMigrations();
    return true;
}

void Database::runMigrations()
{
    QSqlQuery q(m_db);

    // Playlists
    q.exec(QStringLiteral(R"sql(
        CREATE TABLE IF NOT EXISTS playlists (
            id          INTEGER PRIMARY KEY AUTOINCREMENT,
            name        TEXT NOT NULL,
            imported_at TEXT NOT NULL
        )
    )sql"));

    // Songs (tracks)
    q.exec(QStringLiteral(R"sql(
        CREATE TABLE IF NOT EXISTS songs (
            id          INTEGER PRIMARY KEY AUTOINCREMENT,
            title       TEXT,
            artist      TEXT,
            album       TEXT,
            genre       TEXT,
            bpm         REAL,
            rating      INTEGER DEFAULT 0,
            time        TEXT,
            key_sig     TEXT,
            date_added  TEXT,
            format      TEXT DEFAULT 'mp3',
            has_aiff    INTEGER DEFAULT 0,
            match_key   TEXT UNIQUE
        )
    )sql"));

    // Many-to-many
    q.exec(QStringLiteral(R"sql(
        CREATE TABLE IF NOT EXISTS playlist_songs (
            playlist_id INTEGER NOT NULL REFERENCES playlists(id) ON DELETE CASCADE,
            song_id     INTEGER NOT NULL REFERENCES songs(id) ON DELETE CASCADE,
            PRIMARY KEY (playlist_id, song_id)
        )
    )sql"));

    // Downloads
    q.exec(QStringLiteral(R"sql(
        CREATE TABLE IF NOT EXISTS downloads (
            id          INTEGER PRIMARY KEY AUTOINCREMENT,
            filename    TEXT NOT NULL,
            filepath    TEXT NOT NULL UNIQUE,
            extension   TEXT,
            size_mb     REAL DEFAULT 0,
            detected_at TEXT NOT NULL
        )
    )sql"));

    // Conversions
    q.exec(QStringLiteral(R"sql(
        CREATE TABLE IF NOT EXISTS conversions (
            id          INTEGER PRIMARY KEY AUTOINCREMENT,
            download_id INTEGER REFERENCES downloads(id) ON DELETE CASCADE,
            source_path TEXT,
            output_path TEXT,
            source_ext  TEXT,
            status      TEXT DEFAULT 'pending',
            error_msg   TEXT,
            size_mb     REAL DEFAULT 0,
            started_at  TEXT,
            finished_at TEXT
        )
    )sql"));

    // Config
    q.exec(QStringLiteral(R"sql(
        CREATE TABLE IF NOT EXISTS config (
            key   TEXT PRIMARY KEY,
            value TEXT
        )
    )sql"));

    // Migration: add filepath column to songs if it doesn't exist yet.
    {
        bool hasFilepath = false;
        QSqlQuery info(m_db);
        if (info.exec(QStringLiteral("PRAGMA table_info(songs)"))) {
            while (info.next()) {
                if (info.value(1).toString() == QLatin1String("filepath")) {
                    hasFilepath = true;
                    break;
                }
            }
        }
        if (!hasFilepath) {
            q.exec(QStringLiteral("ALTER TABLE songs ADD COLUMN filepath TEXT DEFAULT ''"));
            qInfo() << "Database: migrated songs table — added filepath column";
        }
    }

    // Migration: add Rekordbox-level metadata columns to songs (safe — ignores
    // "duplicate column name" errors so it's idempotent on existing databases).
    {
        auto safeAlter = [&](const QString& sql) {
            QSqlQuery aq(m_db);
            if (!aq.exec(sql)) {
                const QString err = aq.lastError().text();
                if (!err.contains(QLatin1String("duplicate column name"),
                                  Qt::CaseInsensitive)) {
                    qWarning() << "DB migration ALTER warning:" << err;
                }
            }
        };
        safeAlter(QStringLiteral("ALTER TABLE songs ADD COLUMN color_label INTEGER DEFAULT 0"));
        safeAlter(QStringLiteral("ALTER TABLE songs ADD COLUMN bitrate      INTEGER DEFAULT 0"));
        safeAlter(QStringLiteral("ALTER TABLE songs ADD COLUMN comment      TEXT    DEFAULT ''"));
        safeAlter(QStringLiteral("ALTER TABLE songs ADD COLUMN play_count   INTEGER DEFAULT 0"));
        safeAlter(QStringLiteral("ALTER TABLE songs ADD COLUMN date_played  TEXT    DEFAULT ''"));
        safeAlter(QStringLiteral("ALTER TABLE songs ADD COLUMN energy       INTEGER DEFAULT 0"));
    }

    // Migration: Essentia deep analysis columns on songs
    {
        auto safeAlter = [&](const QString& sql) {
            QSqlQuery aq(m_db);
            if (!aq.exec(sql)) {
                const QString err = aq.lastError().text();
                if (!err.contains(QLatin1String("duplicate column name"),
                                  Qt::CaseInsensitive)) {
                    qWarning() << "DB migration ALTER warning:" << err;
                }
            }
        };
        safeAlter(QStringLiteral("ALTER TABLE songs ADD COLUMN mood_tags          TEXT    DEFAULT ''"));
        safeAlter(QStringLiteral("ALTER TABLE songs ADD COLUMN style_tags         TEXT    DEFAULT ''"));
        safeAlter(QStringLiteral("ALTER TABLE songs ADD COLUMN danceability       REAL    DEFAULT 0"));
        safeAlter(QStringLiteral("ALTER TABLE songs ADD COLUMN valence            REAL    DEFAULT 0"));
        safeAlter(QStringLiteral("ALTER TABLE songs ADD COLUMN vocal_prob         REAL    DEFAULT 0"));
        safeAlter(QStringLiteral("ALTER TABLE songs ADD COLUMN essentia_analyzed  INTEGER DEFAULT 0"));
    }

    // Migration: Preparation mode + waveform computed flag
    {
        auto safeAlter = [&](const QString& sql) {
            QSqlQuery aq(m_db);
            if (!aq.exec(sql)) {
                const QString err = aq.lastError().text();
                if (!err.contains(QLatin1String("duplicate column name"),
                                  Qt::CaseInsensitive)) {
                    qWarning() << "DB migration ALTER warning:" << err;
                }
            }
        };
        safeAlter(QStringLiteral("ALTER TABLE songs ADD COLUMN is_prepared INTEGER DEFAULT 0"));
    }

    // Cue points table
    q.exec(QStringLiteral(R"sql(
        CREATE TABLE IF NOT EXISTS cue_points (
            id          INTEGER PRIMARY KEY AUTOINCREMENT,
            song_id     INTEGER NOT NULL REFERENCES songs(id) ON DELETE CASCADE,
            cue_type    TEXT NOT NULL DEFAULT 'hot_cue',
            slot        INTEGER DEFAULT -1,
            position_ms INTEGER NOT NULL DEFAULT 0,
            end_ms      INTEGER DEFAULT -1,
            name        TEXT DEFAULT '',
            color       INTEGER DEFAULT 1,
            sort_order  INTEGER DEFAULT 0
        )
    )sql"));
    q.exec(QStringLiteral(
        "CREATE INDEX IF NOT EXISTS idx_cuepoints_song ON cue_points(song_id)"));

    // Waveform overview cache
    q.exec(QStringLiteral(R"sql(
        CREATE TABLE IF NOT EXISTS waveform_cache (
            song_id      INTEGER PRIMARY KEY REFERENCES songs(id) ON DELETE CASCADE,
            peaks        BLOB NOT NULL,
            generated_at TEXT
        )
    )sql"));

    // Smart playlists
    q.exec(QStringLiteral(R"sql(
        CREATE TABLE IF NOT EXISTS smart_playlists (
            id         INTEGER PRIMARY KEY AUTOINCREMENT,
            name       TEXT NOT NULL,
            rules_json TEXT NOT NULL DEFAULT '{}',
            sort_field TEXT DEFAULT 'title',
            sort_dir   TEXT DEFAULT 'ASC',
            created_at TEXT DEFAULT (datetime('now'))
        )
    )sql"));

    // Play history (for History node in the collection tree)
    q.exec(QStringLiteral(R"sql(
        CREATE TABLE IF NOT EXISTS play_history (
            id          INTEGER PRIMARY KEY AUTOINCREMENT,
            song_id     INTEGER NOT NULL REFERENCES songs(id) ON DELETE CASCADE,
            played_at   TEXT NOT NULL,
            duration_ms INTEGER DEFAULT 0
        )
    )sql"));
    q.exec(QStringLiteral(
        "CREATE INDEX IF NOT EXISTS idx_history_date ON play_history(played_at)"));

    // Schema version tracking
    q.exec(QStringLiteral(R"sql(
        CREATE TABLE IF NOT EXISTS schema_version (
            version    INTEGER PRIMARY KEY,
            applied_at TEXT DEFAULT (datetime('now'))
        )
    )sql"));

    // ── FTS5 full-text search index on songs ─────────────────────────────────
    q.exec(QStringLiteral(R"sql(
        CREATE VIRTUAL TABLE IF NOT EXISTS songs_fts USING fts5(
            title, artist, album, genre, comment,
            content='songs', content_rowid='id'
        )
    )sql"));

    // Populate FTS5 index if empty (first run or after DB wipe).
    {
        QSqlQuery cnt(m_db);
        if (cnt.exec(QStringLiteral("SELECT COUNT(*) FROM songs_fts")) && cnt.next()
            && cnt.value(0).toInt() == 0) {
            q.exec(QStringLiteral(R"sql(
                INSERT INTO songs_fts(rowid, title, artist, album, genre, comment)
                    SELECT id, title, artist, album, genre, comment FROM songs
            )sql"));
            qInfo() << "Database: populated FTS5 index from existing songs";
        }
    }

    // Triggers to keep FTS5 in sync with songs table.
    q.exec(QStringLiteral(R"sql(
        CREATE TRIGGER IF NOT EXISTS songs_fts_insert AFTER INSERT ON songs BEGIN
            INSERT INTO songs_fts(rowid, title, artist, album, genre, comment)
            VALUES (new.id, new.title, new.artist, new.album, new.genre, new.comment);
        END
    )sql"));

    q.exec(QStringLiteral(R"sql(
        CREATE TRIGGER IF NOT EXISTS songs_fts_delete AFTER DELETE ON songs BEGIN
            INSERT INTO songs_fts(songs_fts, rowid, title, artist, album, genre, comment)
            VALUES ('delete', old.id, old.title, old.artist, old.album, old.genre, old.comment);
        END
    )sql"));

    q.exec(QStringLiteral(R"sql(
        CREATE TRIGGER IF NOT EXISTS songs_fts_update AFTER UPDATE ON songs BEGIN
            INSERT INTO songs_fts(songs_fts, rowid, title, artist, album, genre, comment)
            VALUES ('delete', old.id, old.title, old.artist, old.album, old.genre, old.comment);
            INSERT INTO songs_fts(rowid, title, artist, album, genre, comment)
            VALUES (new.id, new.title, new.artist, new.album, new.genre, new.comment);
        END
    )sql"));

    if (q.lastError().isValid())
        qWarning() << "DB migration error:" << q.lastError().text();
}

// ── Playlists ──────────────────────────────────────────────────────────────

QVector<Playlist> Database::loadPlaylists()
{
    QVector<Playlist> result;
    QSqlQuery q(m_db);

    // Load playlists with track counts and format breakdown
    q.prepare(QStringLiteral(R"sql(
        SELECT p.id, p.name, p.imported_at,
               COUNT(ps.song_id) as total
        FROM playlists p
        LEFT JOIN playlist_songs ps ON ps.playlist_id = p.id
        GROUP BY p.id
        ORDER BY p.imported_at DESC
    )sql"));

    if (!q.exec()) {
        qWarning() << "loadPlaylists error:" << q.lastError().text();
        return result;
    }

    while (q.next()) {
        Playlist p;
        p.id          = q.value(0).toLongLong();
        p.name        = q.value(1).toString().toStdString();
        p.imported_at = q.value(2).toString().toStdString();
        p.total       = q.value(3).toInt();
        result.append(p);
    }

    // Load format counts per playlist
    for (auto& p : result) {
        QSqlQuery fq(m_db);
        fq.prepare(QStringLiteral(R"sql(
            SELECT s.format, COUNT(*) as cnt
            FROM songs s
            JOIN playlist_songs ps ON ps.song_id = s.id
            WHERE ps.playlist_id = ?
            GROUP BY s.format
        )sql"));
        fq.addBindValue(static_cast<qlonglong>(p.id));
        if (fq.exec()) {
            while (fq.next()) {
                const std::string fmt = fq.value(0).toString().toStdString();
                p.format_counts[fmt] = fq.value(1).toInt();
            }
        }
    }

    return result;
}

long long Database::insertPlaylist(const QString& name, const QString& importedAt)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("INSERT INTO playlists (name, imported_at) VALUES (?, ?)"));
    q.addBindValue(name);
    q.addBindValue(importedAt);
    if (!q.exec()) {
        m_error = q.lastError().text();
        return -1;
    }
    return q.lastInsertId().toLongLong();
}

bool Database::deletePlaylist(long long id)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("DELETE FROM playlists WHERE id = ?"));
    q.addBindValue(static_cast<qlonglong>(id));
    if (!q.exec()) {
        m_error = q.lastError().text();
        return false;
    }
    return true;
}

// ── Songs ──────────────────────────────────────────────────────────────────

QVector<Track> Database::loadTracks(long long playlistId, int offset, int limit)
{
    QVector<Track> result;
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(R"sql(
        SELECT s.id, s.title, s.artist, s.album, s.genre,
               s.bpm, s.rating, s.time, s.key_sig, s.date_added,
               s.format, s.has_aiff, s.match_key, s.filepath,
               s.color_label, s.bitrate, s.comment, s.play_count, s.date_played, s.energy,
               s.mood_tags, s.style_tags, s.danceability, s.valence, s.vocal_prob, s.essentia_analyzed,
               s.is_prepared
        FROM songs s
        JOIN playlist_songs ps ON ps.song_id = s.id
        WHERE ps.playlist_id = ?
        ORDER BY s.title ASC
        LIMIT ? OFFSET ?
    )sql"));
    q.addBindValue(static_cast<qlonglong>(playlistId));
    q.addBindValue(limit);
    q.addBindValue(offset);

    if (!q.exec()) {
        qWarning() << "loadTracks error:" << q.lastError().text();
        return result;
    }

    while (q.next()) {
        Track t;
        t.id          = q.value(0).toLongLong();
        t.title       = q.value(1).toString().toStdString();
        t.artist      = q.value(2).toString().toStdString();
        t.album       = q.value(3).toString().toStdString();
        t.genre       = q.value(4).toString().toStdString();
        t.bpm         = q.value(5).toDouble();
        t.rating      = q.value(6).toInt();
        t.time        = q.value(7).toString().toStdString();
        t.key_sig     = q.value(8).toString().toStdString();
        t.date_added  = q.value(9).toString().toStdString();
        t.format      = q.value(10).toString().toStdString();
        t.has_aiff    = q.value(11).toInt() != 0;
        t.match_key   = q.value(12).toString().toStdString();
        t.filepath    = q.value(13).toString().toStdString();
        t.color_label = q.value(14).toInt();
        t.bitrate     = q.value(15).toInt();
        t.comment     = q.value(16).toString().toStdString();
        t.play_count  = q.value(17).toInt();
        t.date_played = q.value(18).toString().toStdString();
        t.energy      = q.value(19).toInt();
        t.mood_tags          = q.value(20).toString().toStdString();
        t.style_tags         = q.value(21).toString().toStdString();
        t.danceability       = q.value(22).toFloat();
        t.valence            = q.value(23).toFloat();
        t.vocal_prob         = q.value(24).toFloat();
        t.essentia_analyzed  = q.value(25).toInt() != 0;
        t.is_prepared        = q.value(26).toInt() != 0;
        result.append(t);
    }
    return result;
}

int Database::countTracks(long long playlistId)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(R"sql(
        SELECT COUNT(*) FROM playlist_songs WHERE playlist_id = ?
    )sql"));
    q.addBindValue(static_cast<qlonglong>(playlistId));
    if (q.exec() && q.next())
        return q.value(0).toInt();
    return 0;
}

Track Database::loadSongById(long long id)
{
    Track t;
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(R"sql(
        SELECT id, title, artist, album, genre, bpm, rating, time, key_sig,
               date_added, format, has_aiff, match_key, filepath,
               color_label, bitrate, comment, play_count, date_played, energy,
               mood_tags, style_tags, danceability, valence, vocal_prob, essentia_analyzed,
               is_prepared
        FROM songs WHERE id = ?
    )sql"));
    q.addBindValue(static_cast<qlonglong>(id));
    if (!q.exec() || !q.next()) {
        qWarning() << "Database::loadSongById: failed for id" << id << ":" << q.lastError().text();
        return t;
    }
    t.id         = q.value(0).toLongLong();
    t.title      = q.value(1).toString().toStdString();
    t.artist     = q.value(2).toString().toStdString();
    t.album      = q.value(3).toString().toStdString();
    t.genre      = q.value(4).toString().toStdString();
    t.bpm        = q.value(5).toDouble();
    t.rating     = q.value(6).toInt();
    t.time       = q.value(7).toString().toStdString();
    t.key_sig    = q.value(8).toString().toStdString();
    t.date_added = q.value(9).toString().toStdString();
    t.format     = q.value(10).toString().toStdString();
    t.has_aiff    = q.value(11).toInt() != 0;
    t.match_key   = q.value(12).toString().toStdString();
    t.filepath    = q.value(13).toString().toStdString();
    t.color_label = q.value(14).toInt();
    t.bitrate     = q.value(15).toInt();
    t.comment     = q.value(16).toString().toStdString();
    t.play_count  = q.value(17).toInt();
    t.date_played = q.value(18).toString().toStdString();
    t.energy      = q.value(19).toInt();
    t.mood_tags          = q.value(20).toString().toStdString();
    t.style_tags         = q.value(21).toString().toStdString();
    t.danceability       = q.value(22).toFloat();
    t.valence            = q.value(23).toFloat();
    t.vocal_prob         = q.value(24).toFloat();
    t.essentia_analyzed  = q.value(25).toInt() != 0;
    t.is_prepared        = q.value(26).toInt() != 0;
    return t;
}

Track Database::syncFromDisk(const Track& scanTrack)
{
    QSqlQuery q(m_db);

    // If a row with this match_key already exists, return it with user-edited fields intact.
    q.prepare(QStringLiteral("SELECT id FROM songs WHERE match_key = ?"));
    q.addBindValue(QString::fromStdString(scanTrack.match_key));
    if (q.exec() && q.next()) {
        const long long existingId = q.value(0).toLongLong();
        Track dbTrack = loadSongById(existingId);
        if (dbTrack.id > 0) {
            dbTrack.filepath = scanTrack.filepath;  // always use current on-disk path
            // Persist updated filepath (handles file moves)
            QSqlQuery uq(m_db);
            uq.prepare(QStringLiteral("UPDATE songs SET filepath = ? WHERE id = ?"));
            uq.addBindValue(QString::fromStdString(scanTrack.filepath));
            uq.addBindValue(static_cast<qlonglong>(existingId));
            uq.exec();
            qDebug() << "Database::syncFromDisk: loaded existing id=" << existingId
                     << "match_key=" << QString::fromStdString(scanTrack.match_key);
            return dbTrack;
        }
        // loadSongById failed (should not happen); fall back to scan data with the DB id
        qWarning() << "Database::syncFromDisk: loadSongById failed for id=" << existingId;
        Track fallback = scanTrack;
        fallback.id = existingId;
        return fallback;
    }

    // New track — insert from scan data.
    q.prepare(QStringLiteral(R"sql(
        INSERT INTO songs
            (title, artist, album, genre, bpm, rating, time, key_sig, date_added,
             format, has_aiff, match_key, filepath)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    )sql"));
    q.addBindValue(QString::fromStdString(scanTrack.title));
    q.addBindValue(QString::fromStdString(scanTrack.artist));
    q.addBindValue(QString::fromStdString(scanTrack.album));
    q.addBindValue(QString::fromStdString(scanTrack.genre));
    q.addBindValue(scanTrack.bpm);
    q.addBindValue(scanTrack.rating);
    q.addBindValue(QString::fromStdString(scanTrack.time));
    q.addBindValue(QString::fromStdString(scanTrack.key_sig));
    q.addBindValue(QString::fromStdString(scanTrack.date_added));
    q.addBindValue(QString::fromStdString(scanTrack.format.empty() ? "mp3" : scanTrack.format));
    q.addBindValue(scanTrack.has_aiff ? 1 : 0);
    q.addBindValue(QString::fromStdString(scanTrack.match_key));
    q.addBindValue(QString::fromStdString(scanTrack.filepath));

    if (!q.exec()) {
        // Likely a UNIQUE constraint violation: another file already uses this match_key
        // (e.g. the same track exists in both FLAC and AIFF). Retry with a filepath-based key
        // so each file gets its own DB row.
        const QString fileKey = QStringLiteral("file:") + QString::fromStdString(scanTrack.filepath);
        qWarning() << "Database::syncFromDisk: insert failed, retrying with file key:"
                   << fileKey << "error:" << q.lastError().text();

        // Check if a row with this file key already exists
        QSqlQuery fq(m_db);
        fq.prepare(QStringLiteral("SELECT id FROM songs WHERE match_key = ?"));
        fq.addBindValue(fileKey);
        if (fq.exec() && fq.next()) {
            const long long existingId = fq.value(0).toLongLong();
            Track dbTrack = loadSongById(existingId);
            if (dbTrack.id > 0) {
                dbTrack.filepath = scanTrack.filepath;
                QSqlQuery uq(m_db);
                uq.prepare(QStringLiteral("UPDATE songs SET filepath = ? WHERE id = ?"));
                uq.addBindValue(QString::fromStdString(scanTrack.filepath));
                uq.addBindValue(static_cast<qlonglong>(existingId));
                uq.exec();
                return dbTrack;
            }
        }

        // Insert with file-path key
        QSqlQuery rq(m_db);
        rq.prepare(QStringLiteral(R"sql(
            INSERT INTO songs
                (title, artist, album, genre, bpm, rating, time, key_sig, date_added,
                 format, has_aiff, match_key, filepath)
            VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
        )sql"));
        rq.addBindValue(QString::fromStdString(scanTrack.title));
        rq.addBindValue(QString::fromStdString(scanTrack.artist));
        rq.addBindValue(QString::fromStdString(scanTrack.album));
        rq.addBindValue(QString::fromStdString(scanTrack.genre));
        rq.addBindValue(scanTrack.bpm);
        rq.addBindValue(scanTrack.rating);
        rq.addBindValue(QString::fromStdString(scanTrack.time));
        rq.addBindValue(QString::fromStdString(scanTrack.key_sig));
        rq.addBindValue(QString::fromStdString(scanTrack.date_added));
        rq.addBindValue(QString::fromStdString(scanTrack.format.empty() ? "mp3" : scanTrack.format));
        rq.addBindValue(scanTrack.has_aiff ? 1 : 0);
        rq.addBindValue(fileKey);
        rq.addBindValue(QString::fromStdString(scanTrack.filepath));
        if (!rq.exec()) {
            qWarning() << "Database::syncFromDisk: file-key insert also failed:" << rq.lastError().text();
            Track failed = scanTrack;
            failed.id = -1;
            return failed;
        }
        Track newTrack = scanTrack;
        newTrack.id = rq.lastInsertId().toLongLong();
        newTrack.match_key = fileKey.toStdString();
        qDebug() << "Database::syncFromDisk: inserted with file key, id=" << newTrack.id;
        return newTrack;
    }

    Track newTrack = scanTrack;
    newTrack.id = q.lastInsertId().toLongLong();
    qDebug() << "Database::syncFromDisk: inserted new id=" << newTrack.id
             << "match_key=" << QString::fromStdString(scanTrack.match_key);
    return newTrack;
}

QVector<Track> Database::loadLibrarySongs(const QString& folderPrefix)
{
    QVector<Track> result;
    QSqlQuery q(m_db);

    // Normalise: strip trailing slash so the LIKE pattern is always "prefix/%"
    const QString prefix = folderPrefix.endsWith(QLatin1Char('/'))
                           ? folderPrefix.chopped(1) : folderPrefix;

    q.prepare(QStringLiteral(R"sql(
        SELECT id, title, artist, album, genre, bpm, rating, time, key_sig,
               date_added, format, has_aiff, match_key, filepath,
               color_label, bitrate, comment, play_count, date_played, energy,
               mood_tags, style_tags, danceability, valence, vocal_prob, essentia_analyzed,
               is_prepared
        FROM songs
        WHERE filepath LIKE ?
        ORDER BY title ASC
    )sql"));
    q.addBindValue(prefix + QStringLiteral("/%"));

    if (!q.exec()) {
        qWarning() << "loadLibrarySongs error:" << q.lastError().text();
        return result;
    }
    while (q.next()) {
        Track t;
        t.id         = q.value(0).toLongLong();
        t.title      = q.value(1).toString().toStdString();
        t.artist     = q.value(2).toString().toStdString();
        t.album      = q.value(3).toString().toStdString();
        t.genre      = q.value(4).toString().toStdString();
        t.bpm        = q.value(5).toDouble();
        t.rating     = q.value(6).toInt();
        t.time       = q.value(7).toString().toStdString();
        t.key_sig    = q.value(8).toString().toStdString();
        t.date_added = q.value(9).toString().toStdString();
        t.format     = q.value(10).toString().toStdString();
        t.has_aiff    = q.value(11).toInt() != 0;
        t.match_key   = q.value(12).toString().toStdString();
        t.filepath    = q.value(13).toString().toStdString();
        t.color_label = q.value(14).toInt();
        t.bitrate     = q.value(15).toInt();
        t.comment     = q.value(16).toString().toStdString();
        t.play_count  = q.value(17).toInt();
        t.date_played = q.value(18).toString().toStdString();
        t.energy      = q.value(19).toInt();
        t.mood_tags          = q.value(20).toString().toStdString();
        t.style_tags         = q.value(21).toString().toStdString();
        t.danceability       = q.value(22).toFloat();
        t.valence            = q.value(23).toFloat();
        t.vocal_prob         = q.value(24).toFloat();
        t.essentia_analyzed  = q.value(25).toInt() != 0;
        t.is_prepared        = q.value(26).toInt() != 0;
        result.append(t);
    }
    qInfo() << "Database::loadLibrarySongs:" << result.size()
            << "tracks for" << folderPrefix;
    return result;
}

long long Database::upsertSong(const Track& t)
{
    QSqlQuery q(m_db);

    // Try to find existing by match_key
    q.prepare(QStringLiteral("SELECT id FROM songs WHERE match_key = ?"));
    q.addBindValue(QString::fromStdString(t.match_key));
    if (q.exec() && q.next())
        return q.value(0).toLongLong();

    // Insert new
    q.prepare(QStringLiteral(R"sql(
        INSERT INTO songs
            (title, artist, album, genre, bpm, rating, time, key_sig, date_added,
             format, has_aiff, match_key)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    )sql"));
    q.addBindValue(QString::fromStdString(t.title));
    q.addBindValue(QString::fromStdString(t.artist));
    q.addBindValue(QString::fromStdString(t.album));
    q.addBindValue(QString::fromStdString(t.genre));
    q.addBindValue(t.bpm);
    q.addBindValue(t.rating);
    q.addBindValue(QString::fromStdString(t.time));
    q.addBindValue(QString::fromStdString(t.key_sig));
    q.addBindValue(QString::fromStdString(t.date_added));
    q.addBindValue(QString::fromStdString(t.format));
    q.addBindValue(t.has_aiff ? 1 : 0);
    q.addBindValue(QString::fromStdString(t.match_key));

    if (!q.exec()) {
        m_error = q.lastError().text();
        return -1;
    }
    return q.lastInsertId().toLongLong();
}

bool Database::linkSongToPlaylist(long long songId, long long playlistId)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "INSERT OR IGNORE INTO playlist_songs (playlist_id, song_id) VALUES (?, ?)"));
    q.addBindValue(static_cast<qlonglong>(playlistId));
    q.addBindValue(static_cast<qlonglong>(songId));
    if (!q.exec()) {
        m_error = q.lastError().text();
        return false;
    }
    return true;
}

bool Database::updateSongFormat(long long songId, const QString& format)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("UPDATE songs SET format = ? WHERE id = ?"));
    q.addBindValue(format);
    q.addBindValue(static_cast<qlonglong>(songId));
    if (!q.exec()) {
        m_error = q.lastError().text();
        return false;
    }
    return true;
}

bool Database::bulkUpdateFormat(const QString& format, const QVector<long long>& songIds)
{
    if (songIds.isEmpty()) return true;

    m_db.transaction();
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("UPDATE songs SET format = ? WHERE id = ?"));
    for (long long id : songIds) {
        q.addBindValue(format);
        q.addBindValue(static_cast<qlonglong>(id));
        if (!q.exec()) {
            m_db.rollback();
            m_error = q.lastError().text();
            return false;
        }
    }
    m_db.commit();
    return true;
}

bool Database::updateSongAiff(long long songId, bool hasAiff)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("UPDATE songs SET has_aiff = ? WHERE id = ?"));
    q.addBindValue(hasAiff ? 1 : 0);
    q.addBindValue(static_cast<qlonglong>(songId));
    if (!q.exec()) {
        m_error = q.lastError().text();
        return false;
    }
    return true;
}

bool Database::updateSongColorLabel(long long songId, int colorLabel)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("UPDATE songs SET color_label = ? WHERE id = ?"));
    q.addBindValue(colorLabel);
    q.addBindValue(static_cast<qlonglong>(songId));
    if (!q.exec()) {
        m_error = q.lastError().text();
        return false;
    }
    return true;
}

bool Database::updateSongMetadata(long long songId, const Track& t)
{
    qDebug() << "Database::updateSongMetadata: id=" << songId
             << "title=" << QString::fromStdString(t.title)
             << "artist=" << QString::fromStdString(t.artist);
    const QString matchKey = QString::fromStdString(t.artist).toLower()
                          + QStringLiteral("|||")
                          + QString::fromStdString(t.title).toLower();
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(R"sql(
        UPDATE songs SET
            title = ?, artist = ?, album = ?, genre = ?,
            bpm = ?, time = ?, key_sig = ?, format = ?, match_key = ?
        WHERE id = ?
    )sql"));
    q.addBindValue(QString::fromStdString(t.title));
    q.addBindValue(QString::fromStdString(t.artist));
    q.addBindValue(QString::fromStdString(t.album));
    q.addBindValue(QString::fromStdString(t.genre));
    q.addBindValue(t.bpm);
    q.addBindValue(QString::fromStdString(t.time));
    q.addBindValue(QString::fromStdString(t.key_sig));
    q.addBindValue(QString::fromStdString(t.format.empty() ? "mp3" : t.format));
    q.addBindValue(matchKey);
    q.addBindValue(static_cast<qlonglong>(songId));
    if (!q.exec()) {
        m_error = q.lastError().text();
        qWarning() << "Database::updateSongMetadata failed for id" << songId << ":" << m_error;
        return false;
    }
    return true;
}

QVector<PlaylistMembership> Database::getSongPlaylists(long long songId)
{
    QVector<PlaylistMembership> result;
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(R"sql(
        SELECT p.id, p.name,
               (SELECT COUNT(*) FROM playlist_songs
                WHERE playlist_id = p.id AND song_id = ?) as member
        FROM playlists p
        ORDER BY p.name
    )sql"));
    q.addBindValue(static_cast<qlonglong>(songId));
    if (q.exec()) {
        while (q.next()) {
            PlaylistMembership m;
            m.id     = q.value(0).toLongLong();
            m.name   = q.value(1).toString();
            m.member = q.value(2).toInt() != 0;
            result.append(m);
        }
    }
    return result;
}

bool Database::addSongToPlaylist(long long songId, long long playlistId)
{
    return linkSongToPlaylist(songId, playlistId);
}

bool Database::removeSongFromPlaylist(long long songId, long long playlistId)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "DELETE FROM playlist_songs WHERE playlist_id = ? AND song_id = ?"));
    q.addBindValue(static_cast<qlonglong>(playlistId));
    q.addBindValue(static_cast<qlonglong>(songId));
    if (!q.exec()) {
        m_error = q.lastError().text();
        return false;
    }
    return true;
}

// ── Downloads ──────────────────────────────────────────────────────────────

QVector<Download> Database::loadDownloads()
{
    QVector<Download> result;
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(R"sql(
        SELECT d.id, d.filename, d.filepath, d.extension, d.size_mb, d.detected_at,
               c.id, c.status, c.error_msg
        FROM downloads d
        LEFT JOIN conversions c ON c.download_id = d.id
        ORDER BY d.detected_at DESC
    )sql"));

    if (!q.exec()) {
        qWarning() << "loadDownloads error:" << q.lastError().text();
        return result;
    }

    while (q.next()) {
        Download d;
        d.id          = q.value(0).toLongLong();
        d.filename    = q.value(1).toString().toStdString();
        d.filepath    = q.value(2).toString().toStdString();
        d.extension   = q.value(3).toString().toStdString();
        d.size_mb     = q.value(4).toDouble();
        d.detected_at = q.value(5).toString().toStdString();

        if (!q.value(6).isNull()) {
            d.has_conversion = true;
            d.conv_id        = q.value(6).toLongLong();
            d.conv_status    = convStatusFromString(q.value(7).toString());
            d.conv_error     = q.value(8).toString().toStdString();
        }
        result.append(d);
    }
    return result;
}

long long Database::insertDownload(const QString& filename, const QString& filepath,
                                   const QString& extension, double sizeMb,
                                   const QString& detectedAt)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(R"sql(
        INSERT OR IGNORE INTO downloads (filename, filepath, extension, size_mb, detected_at)
        VALUES (?, ?, ?, ?, ?)
    )sql"));
    q.addBindValue(filename);
    q.addBindValue(filepath);
    q.addBindValue(extension);
    q.addBindValue(sizeMb);
    q.addBindValue(detectedAt);

    if (!q.exec()) {
        m_error = q.lastError().text();
        return -1;
    }
    // If already existed, find it
    if (q.numRowsAffected() == 0) {
        QSqlQuery sq(m_db);
        sq.prepare(QStringLiteral("SELECT id FROM downloads WHERE filepath = ?"));
        sq.addBindValue(filepath);
        if (sq.exec() && sq.next())
            return sq.value(0).toLongLong();
        return -1;
    }
    return q.lastInsertId().toLongLong();
}

bool Database::deleteDownload(long long id)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("DELETE FROM downloads WHERE id = ?"));
    q.addBindValue(static_cast<qlonglong>(id));
    if (!q.exec()) {
        m_error = q.lastError().text();
        return false;
    }
    return true;
}

bool Database::downloadExists(const QString& filepath)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("SELECT COUNT(*) FROM downloads WHERE filepath = ?"));
    q.addBindValue(filepath);
    if (q.exec() && q.next())
        return q.value(0).toInt() > 0;
    return false;
}

// ── Conversions ────────────────────────────────────────────────────────────

long long Database::insertConversion(long long downloadId, const QString& sourcePath,
                                     const QString& outputPath, const QString& sourceExt,
                                     double sizeMb, const QString& startedAt)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(R"sql(
        INSERT INTO conversions
            (download_id, source_path, output_path, source_ext, status, size_mb, started_at)
        VALUES (?, ?, ?, ?, 'pending', ?, ?)
    )sql"));
    q.addBindValue(static_cast<qlonglong>(downloadId));
    q.addBindValue(sourcePath);
    q.addBindValue(outputPath);
    q.addBindValue(sourceExt);
    q.addBindValue(sizeMb);
    q.addBindValue(startedAt);

    if (!q.exec()) {
        m_error = q.lastError().text();
        return -1;
    }
    return q.lastInsertId().toLongLong();
}

bool Database::updateConversionStatus(long long convId, const QString& status,
                                      const QString& errorMsg, const QString& finishedAt)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "UPDATE conversions SET status = ?, error_msg = ?, finished_at = ? WHERE id = ?"));
    q.addBindValue(status);
    q.addBindValue(errorMsg.isEmpty() ? QVariant() : errorMsg);
    q.addBindValue(finishedAt.isEmpty() ? QVariant() : finishedAt);
    q.addBindValue(static_cast<qlonglong>(convId));

    if (!q.exec()) {
        m_error = q.lastError().text();
        return false;
    }
    return true;
}

// ── Config ─────────────────────────────────────────────────────────────────

WatchConfig Database::loadWatchConfig()
{
    WatchConfig cfg;
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("SELECT key, value FROM config WHERE key IN ('watch_folder','output_folder','auto_convert')"));
    if (q.exec()) {
        while (q.next()) {
            const QString key = q.value(0).toString();
            const QString val = q.value(1).toString();
            if (key == QLatin1String("watch_folder"))  cfg.watchFolder  = val;
            else if (key == QLatin1String("output_folder")) cfg.outputFolder = val;
            else if (key == QLatin1String("auto_convert"))  cfg.autoConvert  = (val == QLatin1String("true") || val == QLatin1String("1"));
        }
    }
    return cfg;
}

bool Database::saveWatchConfig(const WatchConfig& cfg)
{
    const QString upsert = QStringLiteral(
        "INSERT INTO config (key, value) VALUES (?, ?) "
        "ON CONFLICT(key) DO UPDATE SET value = excluded.value");

    m_db.transaction();
    QSqlQuery q(m_db);

    auto doUpsert = [&](const QString& key, const QString& value) -> bool {
        q.prepare(upsert);
        q.addBindValue(key);
        q.addBindValue(value);
        return q.exec();
    };

    if (!doUpsert(QStringLiteral("watch_folder"),  cfg.watchFolder) ||
        !doUpsert(QStringLiteral("output_folder"), cfg.outputFolder) ||
        !doUpsert(QStringLiteral("auto_convert"),  cfg.autoConvert ? QStringLiteral("true") : QStringLiteral("false")))
    {
        m_db.rollback();
        m_error = q.lastError().text();
        return false;
    }
    m_db.commit();
    return true;
}

QString Database::loadLibraryFolder()
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("SELECT value FROM config WHERE key = 'library_folder'"));
    if (q.exec() && q.next())
        return q.value(0).toString();
    return {};
}

bool Database::saveLibraryFolder(const QString& folder)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "INSERT INTO config (key, value) VALUES ('library_folder', ?) "
        "ON CONFLICT(key) DO UPDATE SET value = excluded.value"));
    q.addBindValue(folder);
    if (!q.exec()) {
        m_error = q.lastError().text();
        return false;
    }
    return true;
}

// ── FTS5 Search ────────────────────────────────────────────────────────────

QVector<Track> Database::searchTracks(const QString& query)
{
    const QString trimmed = query.trimmed();
    if (trimmed.isEmpty())
        return loadAllSongs();

    // Sanitize for FTS5: wrap each token in double quotes and append * for prefix matching.
    QStringList tokens = trimmed.split(QRegularExpression(QStringLiteral("\\s+")),
                                       Qt::SkipEmptyParts);
    QStringList ftsTerms;
    for (const QString& tok : tokens) {
        QString escaped = tok;
        escaped.replace(QLatin1Char('"'), QLatin1String("\"\""));
        ftsTerms.append(QStringLiteral("\"%1\"*").arg(escaped));
    }
    const QString ftsQuery = ftsTerms.join(QStringLiteral(" "));

    QVector<Track> result;
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(R"sql(
        SELECT s.id, s.title, s.artist, s.album, s.genre,
               s.bpm, s.rating, s.time, s.key_sig, s.date_added,
               s.format, s.has_aiff, s.match_key, s.filepath,
               s.color_label, s.bitrate, s.comment, s.play_count, s.date_played, s.energy,
               s.mood_tags, s.style_tags, s.danceability, s.valence, s.vocal_prob, s.essentia_analyzed,
               s.is_prepared
        FROM songs s
        JOIN songs_fts ON s.id = songs_fts.rowid
        WHERE songs_fts MATCH ?
        ORDER BY rank
    )sql"));
    q.addBindValue(ftsQuery);

    if (!q.exec()) {
        qWarning() << "searchTracks FTS5 error:" << q.lastError().text()
                   << "query:" << ftsQuery;
        return result;
    }

    while (q.next()) {
        Track t;
        t.id          = q.value(0).toLongLong();
        t.title       = q.value(1).toString().toStdString();
        t.artist      = q.value(2).toString().toStdString();
        t.album       = q.value(3).toString().toStdString();
        t.genre       = q.value(4).toString().toStdString();
        t.bpm         = q.value(5).toDouble();
        t.rating      = q.value(6).toInt();
        t.time        = q.value(7).toString().toStdString();
        t.key_sig     = q.value(8).toString().toStdString();
        t.date_added  = q.value(9).toString().toStdString();
        t.format      = q.value(10).toString().toStdString();
        t.has_aiff    = q.value(11).toInt() != 0;
        t.match_key   = q.value(12).toString().toStdString();
        t.filepath    = q.value(13).toString().toStdString();
        t.color_label = q.value(14).toInt();
        t.bitrate     = q.value(15).toInt();
        t.comment     = q.value(16).toString().toStdString();
        t.play_count  = q.value(17).toInt();
        t.date_played = q.value(18).toString().toStdString();
        t.energy      = q.value(19).toInt();
        t.mood_tags          = q.value(20).toString().toStdString();
        t.style_tags         = q.value(21).toString().toStdString();
        t.danceability       = q.value(22).toFloat();
        t.valence            = q.value(23).toFloat();
        t.vocal_prob         = q.value(24).toFloat();
        t.essentia_analyzed  = q.value(25).toInt() != 0;
        t.is_prepared        = q.value(26).toInt() != 0;
        result.append(t);
    }
    qInfo() << "Database::searchTracks:" << result.size() << "results for" << trimmed;
    return result;
}

// ── Missing File Relocator ─────────────────────────────────────────────────

QVector<Track> Database::findMissingTracks()
{
    const QVector<Track> all = loadAllSongs();
    QVector<Track> missing;
    for (const Track& t : all) {
        if (t.filepath.empty())
            continue;
        if (!QFileInfo::exists(QString::fromStdString(t.filepath)))
            missing.append(t);
    }
    qInfo() << "Database::findMissingTracks:" << missing.size()
            << "missing out of" << all.size() << "total";
    return missing;
}

bool Database::updateTrackFilepath(long long songId, const QString& newPath)
{
    const QString format = QFileInfo(newPath).suffix().toLower();
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("UPDATE songs SET filepath = ?, format = ? WHERE id = ?"));
    q.addBindValue(newPath);
    q.addBindValue(format);
    q.addBindValue(static_cast<qlonglong>(songId));
    if (!q.exec()) {
        m_error = q.lastError().text();
        qWarning() << "updateTrackFilepath failed for id" << songId << ":" << m_error;
        return false;
    }
    return true;
}

bool Database::deleteTrack(long long songId)
{
    m_db.transaction();
    QSqlQuery q(m_db);

    q.prepare(QStringLiteral("DELETE FROM playlist_songs WHERE song_id = ?"));
    q.addBindValue(static_cast<qlonglong>(songId));
    if (!q.exec()) {
        m_db.rollback();
        m_error = q.lastError().text();
        qWarning() << "deleteTrack: playlist_songs delete failed:" << m_error;
        return false;
    }

    q.prepare(QStringLiteral("DELETE FROM songs WHERE id = ?"));
    q.addBindValue(static_cast<qlonglong>(songId));
    if (!q.exec()) {
        m_db.rollback();
        m_error = q.lastError().text();
        qWarning() << "deleteTrack: songs delete failed:" << m_error;
        return false;
    }

    m_db.commit();
    return true;
}

// ── Export helpers ──────────────────────────────────────────────────────────

QVector<Track> Database::loadAllSongs()
{
    QVector<Track> result;
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(R"sql(
        SELECT id, title, artist, album, genre, bpm, rating, time, key_sig,
               date_added, format, has_aiff, match_key, filepath,
               color_label, bitrate, comment, play_count, date_played, energy,
               mood_tags, style_tags, danceability, valence, vocal_prob, essentia_analyzed,
               is_prepared
        FROM songs
        ORDER BY title ASC
    )sql"));
    if (!q.exec()) {
        qWarning() << "loadAllSongs error:" << q.lastError().text();
        return result;
    }
    while (q.next()) {
        Track t;
        t.id          = q.value(0).toLongLong();
        t.title       = q.value(1).toString().toStdString();
        t.artist      = q.value(2).toString().toStdString();
        t.album       = q.value(3).toString().toStdString();
        t.genre       = q.value(4).toString().toStdString();
        t.bpm         = q.value(5).toDouble();
        t.rating      = q.value(6).toInt();
        t.time        = q.value(7).toString().toStdString();
        t.key_sig     = q.value(8).toString().toStdString();
        t.date_added  = q.value(9).toString().toStdString();
        t.format      = q.value(10).toString().toStdString();
        t.has_aiff    = q.value(11).toInt() != 0;
        t.match_key   = q.value(12).toString().toStdString();
        t.filepath    = q.value(13).toString().toStdString();
        t.color_label = q.value(14).toInt();
        t.bitrate     = q.value(15).toInt();
        t.comment     = q.value(16).toString().toStdString();
        t.play_count  = q.value(17).toInt();
        t.date_played = q.value(18).toString().toStdString();
        t.energy      = q.value(19).toInt();
        t.mood_tags          = q.value(20).toString().toStdString();
        t.style_tags         = q.value(21).toString().toStdString();
        t.danceability       = q.value(22).toFloat();
        t.valence            = q.value(23).toFloat();
        t.vocal_prob         = q.value(24).toFloat();
        t.essentia_analyzed  = q.value(25).toInt() != 0;
        t.is_prepared        = q.value(26).toInt() != 0;
        result.append(t);
    }
    qInfo() << "Database::loadAllSongs:" << result.size() << "tracks";
    return result;
}

QVector<Track> Database::loadPlaylistSongs(long long playlistId)
{
    QVector<Track> result;
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(R"sql(
        SELECT s.id, s.title, s.artist, s.album, s.genre,
               s.bpm, s.rating, s.time, s.key_sig, s.date_added,
               s.format, s.has_aiff, s.match_key, s.filepath,
               s.color_label, s.bitrate, s.comment, s.play_count, s.date_played, s.energy,
               s.mood_tags, s.style_tags, s.danceability, s.valence, s.vocal_prob, s.essentia_analyzed,
               s.is_prepared
        FROM songs s
        JOIN playlist_songs ps ON ps.song_id = s.id
        WHERE ps.playlist_id = ?
        ORDER BY s.title ASC
    )sql"));
    q.addBindValue(static_cast<qlonglong>(playlistId));
    if (!q.exec()) {
        qWarning() << "loadPlaylistSongs error:" << q.lastError().text();
        return result;
    }
    while (q.next()) {
        Track t;
        t.id          = q.value(0).toLongLong();
        t.title       = q.value(1).toString().toStdString();
        t.artist      = q.value(2).toString().toStdString();
        t.album       = q.value(3).toString().toStdString();
        t.genre       = q.value(4).toString().toStdString();
        t.bpm         = q.value(5).toDouble();
        t.rating      = q.value(6).toInt();
        t.time        = q.value(7).toString().toStdString();
        t.key_sig     = q.value(8).toString().toStdString();
        t.date_added  = q.value(9).toString().toStdString();
        t.format      = q.value(10).toString().toStdString();
        t.has_aiff    = q.value(11).toInt() != 0;
        t.match_key   = q.value(12).toString().toStdString();
        t.filepath    = q.value(13).toString().toStdString();
        t.color_label = q.value(14).toInt();
        t.bitrate     = q.value(15).toInt();
        t.comment     = q.value(16).toString().toStdString();
        t.play_count  = q.value(17).toInt();
        t.date_played = q.value(18).toString().toStdString();
        t.energy      = q.value(19).toInt();
        t.mood_tags          = q.value(20).toString().toStdString();
        t.style_tags         = q.value(21).toString().toStdString();
        t.danceability       = q.value(22).toFloat();
        t.valence            = q.value(23).toFloat();
        t.vocal_prob         = q.value(24).toFloat();
        t.essentia_analyzed  = q.value(25).toInt() != 0;
        t.is_prepared        = q.value(26).toInt() != 0;
        result.append(t);
    }
    return result;
}

bool Database::updateSongAnalysis(long long songId, double bpm, const QString& key,
                                  int bitrate, const QString& duration)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "UPDATE songs SET bpm = ?, key_sig = ?, bitrate = ?, time = ? WHERE id = ?"));
    q.addBindValue(bpm);
    q.addBindValue(key);
    q.addBindValue(bitrate);
    q.addBindValue(duration);
    q.addBindValue(static_cast<qlonglong>(songId));
    if (!q.exec()) {
        m_error = q.lastError().text();
        qWarning() << "updateSongAnalysis failed for id" << songId << ":" << m_error;
        return false;
    }
    return true;
}

// ── Preparation Mode ────────────────────────────────────────────────────────

bool Database::updateSongPrepared(long long songId, bool prepared)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("UPDATE songs SET is_prepared = ? WHERE id = ?"));
    q.addBindValue(prepared ? 1 : 0);
    q.addBindValue(static_cast<qlonglong>(songId));
    if (!q.exec()) {
        m_error = q.lastError().text();
        qWarning() << "updateSongPrepared failed for id" << songId << ":" << m_error;
        return false;
    }
    return true;
}

QVector<Track> Database::loadPreparedTracks()
{
    QVector<Track> result;
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(R"sql(
        SELECT id, title, artist, album, genre, bpm, rating, time, key_sig,
               date_added, format, has_aiff, match_key, filepath,
               color_label, bitrate, comment, play_count, date_played, energy,
               mood_tags, style_tags, danceability, valence, vocal_prob, essentia_analyzed,
               is_prepared
        FROM songs WHERE is_prepared = 1 ORDER BY title ASC
    )sql"));
    if (!q.exec()) {
        qWarning() << "loadPreparedTracks error:" << q.lastError().text();
        return result;
    }
    while (q.next()) {
        Track t;
        t.id               = q.value(0).toLongLong();
        t.title            = q.value(1).toString().toStdString();
        t.artist           = q.value(2).toString().toStdString();
        t.album            = q.value(3).toString().toStdString();
        t.genre            = q.value(4).toString().toStdString();
        t.bpm              = q.value(5).toDouble();
        t.rating           = q.value(6).toInt();
        t.time             = q.value(7).toString().toStdString();
        t.key_sig          = q.value(8).toString().toStdString();
        t.date_added       = q.value(9).toString().toStdString();
        t.format           = q.value(10).toString().toStdString();
        t.has_aiff         = q.value(11).toInt() != 0;
        t.match_key        = q.value(12).toString().toStdString();
        t.filepath         = q.value(13).toString().toStdString();
        t.color_label      = q.value(14).toInt();
        t.bitrate          = q.value(15).toInt();
        t.comment          = q.value(16).toString().toStdString();
        t.play_count       = q.value(17).toInt();
        t.date_played      = q.value(18).toString().toStdString();
        t.energy           = q.value(19).toInt();
        t.mood_tags        = q.value(20).toString().toStdString();
        t.style_tags       = q.value(21).toString().toStdString();
        t.danceability     = q.value(22).toFloat();
        t.valence          = q.value(23).toFloat();
        t.vocal_prob       = q.value(24).toFloat();
        t.essentia_analyzed= q.value(25).toInt() != 0;
        t.is_prepared      = q.value(26).toInt() != 0;
        result.append(t);
    }
    return result;
}

// ── Duplicate Detector ────────────────────────────────────────────────────────

QVector<Database::DuplicatePair> Database::findDuplicateTracks()
{
    QVector<DuplicatePair> result;

    // Find tracks sharing the same artist+title (match_key) but different IDs.
    // We normalise match_key to lower(artist)|||lower(title) on insert,
    // so two rows with the same match_key are effectively the same song.
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(R"sql(
        SELECT a.id, b.id
        FROM songs a
        JOIN songs b ON (
            a.id < b.id AND (
                (a.match_key = b.match_key AND a.match_key NOT LIKE 'file:%')
                OR (
                    lower(a.title) = lower(b.title)
                    AND lower(a.artist) = lower(b.artist)
                    AND a.title != ''
                    AND a.artist != ''
                )
            )
        )
        ORDER BY a.id
    )sql"));

    if (!q.exec()) {
        qWarning() << "findDuplicateTracks error:" << q.lastError().text();
        return result;
    }

    while (q.next()) {
        const long long idA = q.value(0).toLongLong();
        const long long idB = q.value(1).toLongLong();
        Track a = loadSongById(idA);
        Track b = loadSongById(idB);
        if (a.id > 0 && b.id > 0)
            result.append({a, b});
    }

    qInfo() << "findDuplicateTracks:" << result.size() << "pairs";
    return result;
}

// ── Play History ────────────────────────────────────────────────────────────

bool Database::recordPlay(long long songId)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "INSERT INTO play_history (song_id, played_at) VALUES (?, datetime('now','localtime'))"));
    q.addBindValue(static_cast<qlonglong>(songId));
    if (!q.exec()) {
        m_error = q.lastError().text();
        qWarning() << "recordPlay failed for id" << songId << ":" << m_error;
        return false;
    }
    // Also bump the songs.play_count and date_played
    QSqlQuery uq(m_db);
    uq.prepare(QStringLiteral(
        "UPDATE songs SET play_count = play_count + 1, "
        "date_played = datetime('now','localtime') WHERE id = ?"));
    uq.addBindValue(static_cast<qlonglong>(songId));
    uq.exec();
    return true;
}

QStringList Database::loadHistoryDates(int limit)
{
    QStringList dates;
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(R"sql(
        SELECT DISTINCT date(played_at) as d
        FROM play_history
        ORDER BY d DESC
        LIMIT ?
    )sql"));
    q.addBindValue(limit);
    if (q.exec()) {
        while (q.next())
            dates.append(q.value(0).toString());
    }
    return dates;
}

QVector<Track> Database::loadTracksPlayedOn(const QString& date)
{
    QVector<Track> result;
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(R"sql(
        SELECT DISTINCT s.id, s.title, s.artist, s.album, s.genre,
               s.bpm, s.rating, s.time, s.key_sig, s.date_added,
               s.format, s.has_aiff, s.match_key, s.filepath,
               s.color_label, s.bitrate, s.comment, s.play_count, s.date_played, s.energy,
               s.mood_tags, s.style_tags, s.danceability, s.valence, s.vocal_prob,
               s.essentia_analyzed, s.is_prepared
        FROM songs s
        JOIN play_history ph ON ph.song_id = s.id
        WHERE date(ph.played_at) = ?
        ORDER BY ph.played_at DESC
    )sql"));
    q.addBindValue(date);
    if (!q.exec()) {
        qWarning() << "loadTracksPlayedOn error:" << q.lastError().text();
        return result;
    }
    while (q.next()) {
        Track t;
        t.id               = q.value(0).toLongLong();
        t.title            = q.value(1).toString().toStdString();
        t.artist           = q.value(2).toString().toStdString();
        t.album            = q.value(3).toString().toStdString();
        t.genre            = q.value(4).toString().toStdString();
        t.bpm              = q.value(5).toDouble();
        t.rating           = q.value(6).toInt();
        t.time             = q.value(7).toString().toStdString();
        t.key_sig          = q.value(8).toString().toStdString();
        t.date_added       = q.value(9).toString().toStdString();
        t.format           = q.value(10).toString().toStdString();
        t.has_aiff         = q.value(11).toInt() != 0;
        t.match_key        = q.value(12).toString().toStdString();
        t.filepath         = q.value(13).toString().toStdString();
        t.color_label      = q.value(14).toInt();
        t.bitrate          = q.value(15).toInt();
        t.comment          = q.value(16).toString().toStdString();
        t.play_count       = q.value(17).toInt();
        t.date_played      = q.value(18).toString().toStdString();
        t.energy           = q.value(19).toInt();
        t.mood_tags        = q.value(20).toString().toStdString();
        t.style_tags       = q.value(21).toString().toStdString();
        t.danceability     = q.value(22).toFloat();
        t.valence          = q.value(23).toFloat();
        t.vocal_prob       = q.value(24).toFloat();
        t.essentia_analyzed= q.value(25).toInt() != 0;
        t.is_prepared      = q.value(26).toInt() != 0;
        result.append(t);
    }
    return result;
}

QVector<Track> Database::loadRecentlyPlayed(int limit)
{
    QVector<Track> result;
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(R"sql(
        SELECT DISTINCT s.id, s.title, s.artist, s.album, s.genre,
               s.bpm, s.rating, s.time, s.key_sig, s.date_added,
               s.format, s.has_aiff, s.match_key, s.filepath,
               s.color_label, s.bitrate, s.comment, s.play_count, s.date_played, s.energy,
               s.mood_tags, s.style_tags, s.danceability, s.valence, s.vocal_prob,
               s.essentia_analyzed, s.is_prepared
        FROM songs s
        JOIN play_history ph ON ph.song_id = s.id
        ORDER BY ph.played_at DESC
        LIMIT ?
    )sql"));
    q.addBindValue(limit);
    if (!q.exec()) {
        qWarning() << "loadRecentlyPlayed error:" << q.lastError().text();
        return result;
    }
    while (q.next()) {
        Track t;
        t.id               = q.value(0).toLongLong();
        t.title            = q.value(1).toString().toStdString();
        t.artist           = q.value(2).toString().toStdString();
        t.album            = q.value(3).toString().toStdString();
        t.genre            = q.value(4).toString().toStdString();
        t.bpm              = q.value(5).toDouble();
        t.rating           = q.value(6).toInt();
        t.time             = q.value(7).toString().toStdString();
        t.key_sig          = q.value(8).toString().toStdString();
        t.date_added       = q.value(9).toString().toStdString();
        t.format           = q.value(10).toString().toStdString();
        t.has_aiff         = q.value(11).toInt() != 0;
        t.match_key        = q.value(12).toString().toStdString();
        t.filepath         = q.value(13).toString().toStdString();
        t.color_label      = q.value(14).toInt();
        t.bitrate          = q.value(15).toInt();
        t.comment          = q.value(16).toString().toStdString();
        t.play_count       = q.value(17).toInt();
        t.date_played      = q.value(18).toString().toStdString();
        t.energy           = q.value(19).toInt();
        t.mood_tags        = q.value(20).toString().toStdString();
        t.style_tags       = q.value(21).toString().toStdString();
        t.danceability     = q.value(22).toFloat();
        t.valence          = q.value(23).toFloat();
        t.vocal_prob       = q.value(24).toFloat();
        t.essentia_analyzed= q.value(25).toInt() != 0;
        t.is_prepared      = q.value(26).toInt() != 0;
        result.append(t);
    }
    return result;
}

QVector<Track> Database::loadRecentlyAdded(int days)
{
    QVector<Track> result;
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(R"sql(
        SELECT id, title, artist, album, genre, bpm, rating, time, key_sig,
               date_added, format, has_aiff, match_key, filepath,
               color_label, bitrate, comment, play_count, date_played, energy,
               mood_tags, style_tags, danceability, valence, vocal_prob,
               essentia_analyzed, is_prepared
        FROM songs
        WHERE date_added >= date('now', ?)
        ORDER BY date_added DESC
    )sql"));
    q.addBindValue(QStringLiteral("-%1 days").arg(days));
    if (!q.exec()) {
        qWarning() << "loadRecentlyAdded error:" << q.lastError().text();
        return result;
    }
    while (q.next()) {
        Track t;
        t.id               = q.value(0).toLongLong();
        t.title            = q.value(1).toString().toStdString();
        t.artist           = q.value(2).toString().toStdString();
        t.album            = q.value(3).toString().toStdString();
        t.genre            = q.value(4).toString().toStdString();
        t.bpm              = q.value(5).toDouble();
        t.rating           = q.value(6).toInt();
        t.time             = q.value(7).toString().toStdString();
        t.key_sig          = q.value(8).toString().toStdString();
        t.date_added       = q.value(9).toString().toStdString();
        t.format           = q.value(10).toString().toStdString();
        t.has_aiff         = q.value(11).toInt() != 0;
        t.match_key        = q.value(12).toString().toStdString();
        t.filepath         = q.value(13).toString().toStdString();
        t.color_label      = q.value(14).toInt();
        t.bitrate          = q.value(15).toInt();
        t.comment          = q.value(16).toString().toStdString();
        t.play_count       = q.value(17).toInt();
        t.date_played      = q.value(18).toString().toStdString();
        t.energy           = q.value(19).toInt();
        t.mood_tags        = q.value(20).toString().toStdString();
        t.style_tags       = q.value(21).toString().toStdString();
        t.danceability     = q.value(22).toFloat();
        t.valence          = q.value(23).toFloat();
        t.vocal_prob       = q.value(24).toFloat();
        t.essentia_analyzed= q.value(25).toInt() != 0;
        t.is_prepared      = q.value(26).toInt() != 0;
        result.append(t);
    }
    return result;
}

bool Database::updateSongEssentiaAnalysis(long long songId, const QString& moodTags,
                                           const QString& styleTags, float danceability,
                                           float valence, float vocalProb)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(R"sql(
        UPDATE songs SET mood_tags = ?, style_tags = ?, danceability = ?,
                         valence = ?, vocal_prob = ?, essentia_analyzed = 1
        WHERE id = ?
    )sql"));
    q.addBindValue(moodTags);
    q.addBindValue(styleTags);
    q.addBindValue(static_cast<double>(danceability));
    q.addBindValue(static_cast<double>(valence));
    q.addBindValue(static_cast<double>(vocalProb));
    q.addBindValue(static_cast<qlonglong>(songId));
    if (!q.exec()) {
        m_error = q.lastError().text();
        qWarning() << "updateSongEssentiaAnalysis failed for id" << songId << ":" << m_error;
        return false;
    }
    return true;
}

// ── Cue Points ──────────────────────────────────────────────────────────────

static QString cueTypeToString(CueType t)
{
    switch (t) {
    case CueType::Loop:      return QStringLiteral("loop");
    case CueType::MemoryCue: return QStringLiteral("memory_cue");
    default:                 return QStringLiteral("hot_cue");
    }
}

static CueType cueTypeFromString(const QString& s)
{
    if (s == QLatin1String("loop"))       return CueType::Loop;
    if (s == QLatin1String("memory_cue")) return CueType::MemoryCue;
    return CueType::HotCue;
}

QVector<CuePoint> Database::loadCuePoints(long long songId)
{
    QVector<CuePoint> result;
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "SELECT id, song_id, cue_type, slot, position_ms, end_ms, name, color, sort_order "
        "FROM cue_points WHERE song_id = :sid ORDER BY sort_order, position_ms"));
    q.bindValue(QStringLiteral(":sid"), static_cast<qlonglong>(songId));
    if (!q.exec()) {
        qWarning() << "loadCuePoints error:" << q.lastError().text();
        return result;
    }
    while (q.next()) {
        CuePoint c;
        c.id          = q.value(0).toLongLong();
        c.song_id     = q.value(1).toLongLong();
        c.cue_type    = cueTypeFromString(q.value(2).toString());
        c.slot        = q.value(3).toInt();
        c.position_ms = q.value(4).toInt();
        c.end_ms      = q.value(5).toInt();
        c.name        = q.value(6).toString().toStdString();
        c.color       = q.value(7).toInt();
        c.sort_order  = q.value(8).toInt();
        result.append(c);
    }
    return result;
}

bool Database::insertCuePoint(CuePoint& cue)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "INSERT INTO cue_points "
        "(song_id, cue_type, slot, position_ms, end_ms, name, color, sort_order) "
        "VALUES (:sid, :type, :slot, :pos, :end, :name, :color, :ord)"));
    q.bindValue(QStringLiteral(":sid"),   static_cast<qlonglong>(cue.song_id));
    q.bindValue(QStringLiteral(":type"),  cueTypeToString(cue.cue_type));
    q.bindValue(QStringLiteral(":slot"),  cue.slot);
    q.bindValue(QStringLiteral(":pos"),   cue.position_ms);
    q.bindValue(QStringLiteral(":end"),   cue.end_ms);
    q.bindValue(QStringLiteral(":name"),  QString::fromStdString(cue.name));
    q.bindValue(QStringLiteral(":color"), cue.color);
    q.bindValue(QStringLiteral(":ord"),   cue.sort_order);
    if (!q.exec()) {
        qWarning() << "insertCuePoint error:" << q.lastError().text();
        return false;
    }
    cue.id = q.lastInsertId().toLongLong();
    return true;
}

bool Database::updateCuePoint(const CuePoint& cue)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "UPDATE cue_points SET slot=:slot, position_ms=:pos, end_ms=:end, "
        "name=:name, color=:color, sort_order=:ord WHERE id=:id"));
    q.bindValue(QStringLiteral(":slot"),  cue.slot);
    q.bindValue(QStringLiteral(":pos"),   cue.position_ms);
    q.bindValue(QStringLiteral(":end"),   cue.end_ms);
    q.bindValue(QStringLiteral(":name"),  QString::fromStdString(cue.name));
    q.bindValue(QStringLiteral(":color"), cue.color);
    q.bindValue(QStringLiteral(":ord"),   cue.sort_order);
    q.bindValue(QStringLiteral(":id"),    static_cast<qlonglong>(cue.id));
    if (!q.exec()) {
        qWarning() << "updateCuePoint error:" << q.lastError().text();
        return false;
    }
    return true;
}

bool Database::deleteCuePoint(long long cueId)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("DELETE FROM cue_points WHERE id=:id"));
    q.bindValue(QStringLiteral(":id"), static_cast<qlonglong>(cueId));
    return q.exec();
}

bool Database::deleteAllCuePoints(long long songId)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("DELETE FROM cue_points WHERE song_id=:sid"));
    q.bindValue(QStringLiteral(":sid"), static_cast<qlonglong>(songId));
    return q.exec();
}

// ── Waveform Cache ──────────────────────────────────────────────────────────

QByteArray Database::loadWaveformOverview(long long songId)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("SELECT peaks FROM waveform_cache WHERE song_id=:sid"));
    q.bindValue(QStringLiteral(":sid"), static_cast<qlonglong>(songId));
    if (!q.exec() || !q.next())
        return {};
    return q.value(0).toByteArray();
}

bool Database::saveWaveformOverview(long long songId, const QByteArray& peaks)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "INSERT OR REPLACE INTO waveform_cache (song_id, peaks, generated_at) "
        "VALUES (:sid, :peaks, datetime('now'))"));
    q.bindValue(QStringLiteral(":sid"),   static_cast<qlonglong>(songId));
    q.bindValue(QStringLiteral(":peaks"), peaks);
    if (!q.exec()) {
        qWarning() << "saveWaveformOverview error:" << q.lastError().text();
        return false;
    }
    return true;
}
