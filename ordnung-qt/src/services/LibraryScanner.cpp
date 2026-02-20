#include "LibraryScanner.h"

#include <QDirIterator>
#include <QFileInfo>
#include <QProcess>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>
#include <QElapsedTimer>
#include <algorithm>

static const QStringList kAudioExts = {
    "mp3","flac","wav","aiff","aif","alac","ogg","m4a","wma","aac","opus","mp4"
};

const QStringList& LibraryScanner::audioExtensions()
{
    return kAudioExts;
}

// Extract time, BPM, key from audio file via ffprobe. Fills in only when available.
static void extractMetadata(const QString& filepath, Track& t)
{
    QProcess proc;
    proc.setProgram(QStringLiteral("ffprobe"));
    proc.setArguments({
        QStringLiteral("-v"), QStringLiteral("quiet"),
        QStringLiteral("-print_format"), QStringLiteral("json"),
        QStringLiteral("-show_format"),
        filepath
    });
    proc.start(QProcess::ReadOnly);
    if (!proc.waitForFinished(5000)) {
        qWarning() << "[LibraryScanner] ffprobe timed out for:" << filepath;
        proc.kill();
        return;
    }
    if (proc.exitCode() != 0) {
        qDebug() << "[LibraryScanner] ffprobe non-zero exit" << proc.exitCode() << "for:" << filepath;
        return;
    }

    const QByteArray out = proc.readAllStandardOutput();
    const QJsonDocument doc = QJsonDocument::fromJson(out);
    if (!doc.isObject())
        return;

    const QJsonObject format = doc.object().value(QStringLiteral("format")).toObject();
    if (format.isEmpty())
        return;

    // Duration (seconds) -> "M:SS"
    const double durSec = format.value(QStringLiteral("duration")).toString().toDouble();
    if (durSec > 0) {
        const int m = static_cast<int>(durSec) / 60;
        const int s = static_cast<int>(durSec) % 60;
        t.time = QStringLiteral("%1:%2").arg(m).arg(s, 2, 10, QChar('0')).toStdString();
    }

    const QJsonObject tags = format.value(QStringLiteral("tags")).toObject();
    if (tags.isEmpty())
        return;

    // BPM: TBPM (ID3), BPM (Vorbis/FLAC)
    for (const QString& key : { QStringLiteral("TBPM"), QStringLiteral("BPM") }) {
        const QString val = tags.value(key).toString().trimmed();
        if (!val.isEmpty()) {
            bool ok = false;
            const double bpm = val.toDouble(&ok);
            if (ok && bpm > 0 && bpm < 999) {
                t.bpm = bpm;
                break;
            }
        }
    }

    // Key: TKEY (ID3), KEY, INITIALKEY (Vorbis/Rekordbox)
    for (const QString& key : { QStringLiteral("TKEY"), QStringLiteral("KEY"),
                                QStringLiteral("INITIALKEY"), QStringLiteral("initial_key") }) {
        const QString val = tags.value(key).toString().trimmed();
        if (!val.isEmpty()) {
            t.key_sig = val.toStdString();
            break;
        }
    }
}

QVector<Track> LibraryScanner::scanFast(const QString& folder)
{
    QVector<Track> result;
    if (folder.isEmpty()) return result;

    qInfo() << "[LibraryScanner] Fast-scanning folder:" << folder;
    QElapsedTimer timer;
    timer.start();

    QDirIterator it(folder, QDir::Files, QDirIterator::Subdirectories);
    long long nextId = 1;

    while (it.hasNext()) {
        const QString path = it.next();
        const QFileInfo info(path);
        const QString ext = info.suffix().toLower();
        if (!kAudioExts.contains(ext)) continue;

        Track t;
        t.id       = nextId++;
        t.filepath = path.toStdString();
        t.format   = ext.toStdString();

        const QString stem   = info.completeBaseName();
        const int     sepIdx = stem.indexOf(QStringLiteral(" - "));
        if (sepIdx > 0) {
            t.artist = stem.left(sepIdx).trimmed().toStdString();
            t.title  = stem.mid(sepIdx + 3).trimmed().toStdString();
        } else {
            t.title = stem.toStdString();
        }

        result.append(t);
    }

    std::sort(result.begin(), result.end(), [](const Track& a, const Track& b) {
        if (a.artist != b.artist) return a.artist < b.artist;
        return a.title < b.title;
    });

    qInfo() << "[LibraryScanner] Fast scan complete:" << result.size()
            << "audio files in" << timer.elapsed() << "ms";
    return result;
}

QVector<Track> LibraryScanner::scan(const QString& folder)
{
    QVector<Track> result;
    if (folder.isEmpty()) return result;

    qInfo() << "[LibraryScanner] Scanning folder:" << folder;
    QElapsedTimer timer;
    timer.start();

    QDirIterator it(folder, QDir::Files, QDirIterator::Subdirectories);
    long long nextId = 1;

    while (it.hasNext()) {
        const QString path = it.next();
        const QFileInfo info(path);
        const QString ext = info.suffix().toLower();
        if (!kAudioExts.contains(ext)) continue;

        Track t;
        t.id       = nextId++;
        t.filepath = path.toStdString();
        t.format   = ext.toStdString();

        // Parse "Artist - Title" from filename, fallback to full stem as title
        const QString stem   = info.completeBaseName();
        const int     sepIdx = stem.indexOf(QStringLiteral(" - "));
        if (sepIdx > 0) {
            t.artist = stem.left(sepIdx).trimmed().toStdString();
            t.title  = stem.mid(sepIdx + 3).trimmed().toStdString();
        } else {
            t.title = stem.toStdString();
        }

        extractMetadata(path, t);
        result.append(t);
    }

    std::sort(result.begin(), result.end(), [](const Track& a, const Track& b) {
        if (a.artist != b.artist) return a.artist < b.artist;
        return a.title < b.title;
    });

    qInfo() << "[LibraryScanner] Scan complete:" << result.size() << "audio files in"
            << timer.elapsed() << "ms";
    return result;
}
