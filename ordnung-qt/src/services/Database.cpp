#include "Database.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QSqlRecord>
#include <QStandardPaths>
#include <QDir>
#include <QDateTime>
#include <QDebug>
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
        return false;
    }

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
               s.format, s.has_aiff, s.match_key
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
