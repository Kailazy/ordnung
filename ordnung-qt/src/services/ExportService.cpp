#include "ExportService.h"
#include "Database.h"
#include "PdbWriter.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMap>
#include <QXmlStreamWriter>
#include <QtConcurrent>
#include <QDebug>

// ── Constructor ─────────────────────────────────────────────────────────────

ExportService::ExportService(Database* db, QObject* parent)
    : QObject(parent)
    , m_db(db)
{}

// ── Public API ──────────────────────────────────────────────────────────────

void ExportService::startExport(const ExportOptions& opts)
{
    m_cancelled.store(false);

    // Capture a pointer for the lambda (ExportService must outlive the future).
    (void)QtConcurrent::run([this, opts]() {
        // 1. Gather playlists
        QVector<Playlist> playlists = m_db->loadPlaylists();
        if (!opts.playlistIds.isEmpty()) {
            QVector<Playlist> filtered;
            for (const auto& p : playlists) {
                if (opts.playlistIds.contains(p.id))
                    filtered.append(p);
            }
            playlists = filtered;
        }

        // 2. Gather tracks per playlist and build a de-duplicated master list
        QMap<long long, QVector<Track>> playlistTracks;
        QMap<long long, Track> seenTracks;  // id -> Track

        for (const auto& pl : playlists) {
            QVector<Track> tracks = m_db->loadPlaylistSongs(pl.id);
            playlistTracks[pl.id] = tracks;
            for (const auto& t : tracks)
                seenTracks.insert(t.id, t);
        }

        QVector<Track> allTracks;
        allTracks.reserve(seenTracks.size());
        for (auto it = seenTracks.cbegin(); it != seenTracks.cend(); ++it)
            allTracks.append(it.value());

        // 3. Load cue points for every track
        QMap<long long, QVector<CuePoint>> cueMap;
        for (const auto& t : allTracks)
            cueMap[t.id] = m_db->loadCuePoints(t.id);

        // 4. Dispatch to the appropriate writer
        QString err;
        if (opts.target == ExportOptions::RekordboxXml) {
            err = writeRekordboxXml(opts.outputPath, playlists, playlistTracks,
                                    allTracks, cueMap);
        } else {
            err = writeCdjUsb(opts, playlists, playlistTracks, allTracks, cueMap);
        }

        emit finished(err.isEmpty(), err);
    });
}

void ExportService::cancel()
{
    m_cancelled.store(true);
}

// ── Rekordbox XML Writer ────────────────────────────────────────────────────

QString ExportService::writeRekordboxXml(
    const QString& xmlPath,
    const QVector<Playlist>& playlists,
    const QMap<long long, QVector<Track>>& playlistTracks,
    const QVector<Track>& allTracks,
    const QMap<long long, QVector<CuePoint>>& cueMap)
{
    QFile file(xmlPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return QStringLiteral("Cannot open file for writing: ") + xmlPath;

    QXmlStreamWriter xml(&file);
    xml.setAutoFormatting(true);
    xml.setAutoFormattingIndent(2);

    xml.writeStartDocument(QStringLiteral("1.0"));

    // Root element
    xml.writeStartElement(QStringLiteral("DJ_PLAYLISTS"));
    xml.writeAttribute(QStringLiteral("Version"), QStringLiteral("1.0.0"));

    // Product info
    xml.writeEmptyElement(QStringLiteral("PRODUCT"));
    xml.writeAttribute(QStringLiteral("Name"),    QStringLiteral("rekordbox"));
    xml.writeAttribute(QStringLiteral("Version"), QStringLiteral("6.0.0"));
    xml.writeAttribute(QStringLiteral("Company"), QStringLiteral("Pioneer DJ"));

    // COLLECTION
    xml.writeStartElement(QStringLiteral("COLLECTION"));
    xml.writeAttribute(QStringLiteral("Entries"), QString::number(allTracks.size()));

    const int total = allTracks.size();
    int done = 0;

    for (const auto& t : allTracks) {
        if (m_cancelled.load())
            return QStringLiteral("Export cancelled by user");

        xml.writeStartElement(QStringLiteral("TRACK"));
        xml.writeAttribute(QStringLiteral("TrackID"),    QString::number(t.id));
        xml.writeAttribute(QStringLiteral("Name"),       QString::fromStdString(t.title));
        xml.writeAttribute(QStringLiteral("Artist"),     QString::fromStdString(t.artist));
        xml.writeAttribute(QStringLiteral("Album"),      QString::fromStdString(t.album));
        xml.writeAttribute(QStringLiteral("Genre"),      QString::fromStdString(t.genre));

        // Parse "M:SS" duration to total seconds
        int totalSeconds = 0;
        {
            const QString ts = QString::fromStdString(t.time);
            const QStringList parts = ts.split(QLatin1Char(':'));
            if (parts.size() == 2)
                totalSeconds = parts[0].toInt() * 60 + parts[1].toInt();
        }
        xml.writeAttribute(QStringLiteral("TotalTime"),  QString::number(totalSeconds));

        // BPM with 2 decimal places
        xml.writeAttribute(QStringLiteral("Bpm"),        QString::number(t.bpm, 'f', 2));
        xml.writeAttribute(QStringLiteral("AverageBpm"), QString::number(t.bpm, 'f', 2));

        // Rating: 0-5 stars -> Rekordbox scale
        xml.writeAttribute(QStringLiteral("Rating"),     QString::number(ratingToRekordbox(t.rating)));
        xml.writeAttribute(QStringLiteral("PlayCount"),  QString::number(t.play_count));
        xml.writeAttribute(QStringLiteral("Tonality"),   QString::fromStdString(t.key_sig));
        xml.writeAttribute(QStringLiteral("BitRate"),    QString::number(t.bitrate));
        xml.writeAttribute(QStringLiteral("ColorID"),    QString::number(t.color_label));
        xml.writeAttribute(QStringLiteral("Comments"),   QString::fromStdString(t.comment));
        xml.writeAttribute(QStringLiteral("DateAdded"),  QString::fromStdString(t.date_added));

        // Location: file://localhost/path  (works on both Linux/Mac and Rekordbox)
        const QString location = QStringLiteral("file://localhost/")
            + QString::fromStdString(t.filepath);
        xml.writeAttribute(QStringLiteral("Location"), location);

        // Write cue points as POSITION_MARK elements
        const auto& cues = cueMap.value(t.id);
        for (const auto& cue : cues) {
            xml.writeEmptyElement(QStringLiteral("POSITION_MARK"));
            xml.writeAttribute(QStringLiteral("Name"),
                               QString::fromStdString(cue.name));

            // CueType -> Type attribute
            if (cue.cue_type == CueType::Loop) {
                xml.writeAttribute(QStringLiteral("Type"), QStringLiteral("4"));
            } else {
                xml.writeAttribute(QStringLiteral("Type"), QStringLiteral("0"));
            }

            // Start position: ms -> seconds
            xml.writeAttribute(QStringLiteral("Start"),
                               QString::number(cue.position_ms / 1000.0, 'f', 3));

            // End position for loops
            if (cue.cue_type == CueType::Loop && cue.end_ms >= 0) {
                xml.writeAttribute(QStringLiteral("End"),
                                   QString::number(cue.end_ms / 1000.0, 'f', 3));
            }

            // Num: slot for hot cues (0-7), -1 for memory cues
            if (cue.cue_type == CueType::HotCue) {
                xml.writeAttribute(QStringLiteral("Num"), QString::number(cue.slot));
            } else {
                xml.writeAttribute(QStringLiteral("Num"), QStringLiteral("-1"));
            }

            // Color RGB from Pioneer palette
            int r = 0, g = 0, b = 0;
            pioneerColorToRgb(cue.color, r, g, b);
            xml.writeAttribute(QStringLiteral("Red"),   QString::number(r));
            xml.writeAttribute(QStringLiteral("Green"), QString::number(g));
            xml.writeAttribute(QStringLiteral("Blue"),  QString::number(b));
        }

        xml.writeEndElement(); // TRACK

        ++done;
        emit progress(ExportProgress{done, total, QString::fromStdString(t.title)});
    }

    xml.writeEndElement(); // COLLECTION

    // PLAYLISTS
    xml.writeStartElement(QStringLiteral("PLAYLISTS"));

    // ROOT node (Type=0 is a folder node)
    xml.writeStartElement(QStringLiteral("NODE"));
    xml.writeAttribute(QStringLiteral("Type"), QStringLiteral("0"));
    xml.writeAttribute(QStringLiteral("Name"), QStringLiteral("ROOT"));

    for (const auto& pl : playlists) {
        const auto& tracks = playlistTracks.value(pl.id);
        xml.writeStartElement(QStringLiteral("NODE"));
        xml.writeAttribute(QStringLiteral("Name"),    QString::fromStdString(pl.name));
        xml.writeAttribute(QStringLiteral("Type"),    QStringLiteral("1"));
        xml.writeAttribute(QStringLiteral("Entries"), QString::number(tracks.size()));
        xml.writeAttribute(QStringLiteral("KeyType"), QStringLiteral("0"));

        for (const auto& t : tracks) {
            xml.writeEmptyElement(QStringLiteral("TRACK"));
            xml.writeAttribute(QStringLiteral("Key"), QString::number(t.id));
        }

        xml.writeEndElement(); // NODE (playlist)
    }

    xml.writeEndElement(); // NODE (ROOT)
    xml.writeEndElement(); // PLAYLISTS
    xml.writeEndElement(); // DJ_PLAYLISTS
    xml.writeEndDocument();

    file.close();
    if (file.error() != QFile::NoError)
        return QStringLiteral("Write error: ") + file.errorString();

    qInfo() << "ExportService: wrote Rekordbox XML with" << allTracks.size()
            << "tracks to" << xmlPath;
    return {};
}

// ── CDJ USB Writer ──────────────────────────────────────────────────────────

QString ExportService::writeCdjUsb(
    const ExportOptions& opts,
    const QVector<Playlist>& playlists,
    const QMap<long long, QVector<Track>>& playlistTracks,
    const QVector<Track>& allTracks,
    const QMap<long long, QVector<CuePoint>>& cueMap)
{
    const QDir usb(opts.outputPath);
    if (!usb.exists())
        return QStringLiteral("USB mount point does not exist: ") + opts.outputPath;

    // Create PIONEER directory structure
    const QString pioneerDir   = opts.outputPath + QStringLiteral("/PIONEER");
    const QString rekordboxDir = pioneerDir + QStringLiteral("/rekordbox");
    const QString anlzDir      = pioneerDir + QStringLiteral("/USBANLZ");

    QDir().mkpath(rekordboxDir);
    QDir().mkpath(anlzDir);

    // Build a map of potentially updated locations (when files are copied)
    QVector<Track> exportTracks = allTracks;
    QMap<long long, QVector<Track>> exportPlaylistTracks = playlistTracks;

    const int total = allTracks.size();
    int done = 0;

    if (opts.copyFiles) {
        const QString contentsDir = opts.outputPath + QStringLiteral("/Contents");

        for (int i = 0; i < exportTracks.size(); ++i) {
            if (m_cancelled.load())
                return QStringLiteral("Export cancelled by user");

            auto& t = exportTracks[i];
            const QFileInfo srcInfo(QString::fromStdString(t.filepath));
            if (!srcInfo.exists())
                continue;

            // Copy to Contents/<Artist>/<filename>
            const QString artistDir = contentsDir + QStringLiteral("/")
                + QString::fromStdString(t.artist.empty() ? "Unknown" : t.artist);
            QDir().mkpath(artistDir);

            const QString destPath = artistDir + QStringLiteral("/") + srcInfo.fileName();
            if (!QFile::exists(destPath))
                QFile::copy(srcInfo.absoluteFilePath(), destPath);

            // Update the filepath to the USB-relative location
            t.filepath = destPath.toStdString();

            ++done;
            emit progress(ExportProgress{done, total, srcInfo.fileName()});
        }

        // Update playlist track references to the new paths
        for (auto it = exportPlaylistTracks.begin(); it != exportPlaylistTracks.end(); ++it) {
            for (auto& t : it.value()) {
                for (const auto& et : exportTracks) {
                    if (et.id == t.id) {
                        t.filepath = et.filepath;
                        break;
                    }
                }
            }
        }
    }

    // Write export.pdb (Pioneer binary database) inside PIONEER/rekordbox/
    const QString pdbPath = rekordboxDir + QStringLiteral("/export.pdb");
    QString pdbErr = PdbWriter::write(pdbPath, playlists, exportPlaylistTracks,
                                      exportTracks, cueMap);
    if (!pdbErr.isEmpty())
        return pdbErr;

    // Write minimal ANLZ stubs for each track
    for (const auto& t : allTracks) {
        if (m_cancelled.load())
            return QStringLiteral("Export cancelled by user");

        const QString trackDir = anlzDir + QStringLiteral("/")
            + QStringLiteral("%1").arg(t.id, 8, 10, QLatin1Char('0'));
        QDir().mkpath(trackDir);

        if (!writeAnlzStub(trackDir + QStringLiteral("/ANLZ0000.DAT")))
            qWarning() << "ExportService: failed to write ANLZ DAT for track" << t.id;
        if (!writeAnlzStub(trackDir + QStringLiteral("/ANLZ0000.EXT")))
            qWarning() << "ExportService: failed to write ANLZ EXT for track" << t.id;
    }

    qInfo() << "ExportService: CDJ USB export complete at" << opts.outputPath;
    return {};
}

// ── ANLZ Stub ───────────────────────────────────────────────────────────────

bool ExportService::writeAnlzStub(const QString& path)
{
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly))
        return false;

    // Magic bytes: "MVEM" (0x4D 0x56 0x45 0x4D)
    const char magic[] = { 0x4D, 0x56, 0x45, 0x4D };
    f.write(magic, 4);

    // 4-byte big-endian length = 32 (header size: 4 magic + 4 length + 24 zeros)
    const char length[] = { 0x00, 0x00, 0x00, 0x20 };
    f.write(length, 4);

    // 24 bytes of zeros (padding to reach 32 bytes total)
    const char zeros[24] = {};
    f.write(zeros, 24);

    f.close();
    return f.error() == QFile::NoError;
}

// ── Rating Conversion ───────────────────────────────────────────────────────

int ExportService::ratingToRekordbox(int stars)
{
    // 0-5 stars -> 0, 51, 102, 153, 204, 255
    static constexpr int map[] = { 0, 51, 102, 153, 204, 255 };
    if (stars < 0 || stars > 5) return 0;
    return map[stars];
}

// ── Pioneer Color Palette ───────────────────────────────────────────────────

void ExportService::pioneerColorToRgb(int colorIndex, int& r, int& g, int& b)
{
    // Pioneer CDJ color indices 1-8 with their standard RGB values.
    switch (colorIndex) {
    case 1: r = 235; g =  20; b =  80; break;  // Pink
    case 2: r = 235; g =   0; b =   0; break;  // Red
    case 3: r = 255; g = 128; b =   0; break;  // Orange
    case 4: r = 232; g = 212; b =   0; break;  // Yellow
    case 5: r =   0; g = 200; b =  50; break;  // Green
    case 6: r =   0; g = 200; b = 200; break;  // Aqua
    case 7: r =   0; g =  80; b = 220; break;  // Blue
    case 8: r = 150; g =   0; b = 220; break;  // Purple
    default: r = 40; g = 226; b =  20; break;   // Default green (Rekordbox default)
    }
}
