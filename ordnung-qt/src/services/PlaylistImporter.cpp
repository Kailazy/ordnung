#include "PlaylistImporter.h"

#include <QFile>
#include <QTextStream>
#include <QStringList>
#include <QRegularExpression>
#include <QDebug>

PlaylistImporter::PlaylistImporter(QObject* parent)
    : QObject(parent)
{}

ImportResult PlaylistImporter::parse(const QString& filePath)
{
    ImportResult result;

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        result.error = QStringLiteral("Cannot open file: ") + filePath;
        return result;
    }

    QTextStream stream(&file);

    // Auto-detect encoding by reading the BOM.
    // Qt 6 defaults to UTF-8 but can handle UTF-16 via the BOM.
    stream.setAutoDetectUnicode(true);

    // Read all content — small files, safe to do at once.
    const QString content = stream.readAll();
    file.close();

    if (content.isEmpty()) {
        result.error = QStringLiteral("File is empty");
        return result;
    }

    // Split into lines — Rekordbox exports may use \r\n or \n
    const QStringList lines = content.split(QRegularExpression(QStringLiteral("\\r?\\n")),
                                            Qt::SkipEmptyParts);

    if (lines.isEmpty()) {
        result.error = QStringLiteral("No lines found");
        return result;
    }

    // Find the header row — first line that contains "Name" or "Title" tab-delimited.
    // Rekordbox exports use a header row like:
    //   #  Name  Artist  Composer  Album  Grouping  Genre  Kind  Size  ...
    int headerLineIdx = -1;
    QStringList headers;

    for (int i = 0; i < qMin(lines.size(), 5); ++i) {
        const QStringList parts = lines[i].split(QLatin1Char('\t'));
        if (parts.size() >= 4) {
            for (const QString& p : parts) {
                if (p.trimmed().compare(QLatin1String("Name"), Qt::CaseInsensitive) == 0 ||
                    p.trimmed().compare(QLatin1String("Title"), Qt::CaseInsensitive) == 0)
                {
                    headerLineIdx = i;
                    headers = parts;
                    break;
                }
            }
            if (headerLineIdx >= 0) break;
        }
    }

    if (headerLineIdx < 0 || headers.isEmpty()) {
        // Fallback: treat first line as header
        headerLineIdx = 0;
        headers = lines[0].split(QLatin1Char('\t'));
    }

    // Normalise header names for lookup
    QStringList normalHeaders;
    for (const QString& h : headers)
        normalHeaders.append(h.trimmed().toLower());

    // Column index resolution
    auto colIdx = [&](const QStringList& candidates) -> int {
        for (const QString& c : candidates) {
            int idx = normalHeaders.indexOf(c);
            if (idx >= 0) return idx;
        }
        return -1;
    };

    const int iTitle     = colIdx({QStringLiteral("name"),        QStringLiteral("title"),  QStringLiteral("track title")});
    const int iArtist    = colIdx({QStringLiteral("artist"),      QStringLiteral("artist name")});
    const int iAlbum     = colIdx({QStringLiteral("album"),       QStringLiteral("album title")});
    const int iGenre     = colIdx({QStringLiteral("genre")});
    const int iBpm       = colIdx({QStringLiteral("bpm"),         QStringLiteral("tempo")});
    const int iRating    = colIdx({QStringLiteral("rating"),      QStringLiteral("my rating")});
    const int iTime      = colIdx({QStringLiteral("time"),        QStringLiteral("total time"), QStringLiteral("duration")});
    const int iKey       = colIdx({QStringLiteral("key"),         QStringLiteral("key sig"), QStringLiteral("tonality")});
    const int iDateAdded = colIdx({QStringLiteral("date added"),  QStringLiteral("date_added"), QStringLiteral("added")});
    const int iFormat    = colIdx({QStringLiteral("kind"),        QStringLiteral("file kind"), QStringLiteral("format")});

    // Parse data rows
    for (int i = headerLineIdx + 1; i < lines.size(); ++i) {
        const QString& line = lines[i];
        if (line.trimmed().isEmpty()) continue;
        // Skip comment/metadata lines that start with #
        if (line.startsWith(QLatin1Char('#'))) continue;

        const QStringList fields = line.split(QLatin1Char('\t'));

        auto field = [&](int idx) -> QString {
            if (idx < 0 || idx >= fields.size()) return QString();
            return fields[idx].trimmed();
        };

        Track t;
        t.title     = field(iTitle).toStdString();
        t.artist    = field(iArtist).toStdString();
        t.album     = field(iAlbum).toStdString();
        t.genre     = field(iGenre).toStdString();
        t.time      = field(iTime).toStdString();
        t.key_sig   = field(iKey).toStdString();
        t.date_added = field(iDateAdded).toStdString();

        // BPM: may be a float string like "128.00"
        const QString bpmStr = field(iBpm);
        if (!bpmStr.isEmpty()) {
            bool ok = false;
            double b = bpmStr.toDouble(&ok);
            if (ok) t.bpm = b;
        }

        // Rating: Rekordbox uses 0-255 scale sometimes; normalise to 0-5
        const QString ratingStr = field(iRating);
        if (!ratingStr.isEmpty()) {
            bool ok = false;
            int r = ratingStr.toInt(&ok);
            if (ok) {
                if (r > 5) r = (r * 5) / 255;  // 255 → 5
                t.rating = qBound(0, r, 5);
            }
        }

        // Format: Rekordbox puts "FLAC File", "AIFF Audio File", etc.
        QString fmt = field(iFormat).toLower();
        if      (fmt.contains(QLatin1String("flac"))) t.format = "flac";
        else if (fmt.contains(QLatin1String("aiff")) || fmt.contains(QLatin1String("aif"))) t.format = "aiff";
        else if (fmt.contains(QLatin1String("wav")))  t.format = "wav";
        else if (fmt.contains(QLatin1String("alac")) || fmt.contains(QLatin1String("apple lossless"))) t.format = "alac";
        else if (fmt.contains(QLatin1String("m4a")) || fmt.contains(QLatin1String("aac"))) t.format = "m4a";
        else if (fmt.contains(QLatin1String("ogg")) || fmt.contains(QLatin1String("vorbis"))) t.format = "ogg";
        else if (fmt.contains(QLatin1String("wma"))) t.format = "wma";
        else                                          t.format = "mp3";  // default

        // Skip rows with no title
        if (t.title.empty()) continue;

        t.match_key = makeMatchKey(
            QString::fromStdString(t.artist),
            QString::fromStdString(t.title)).toStdString();

        result.tracks.append(t);
    }

    result.ok = true;
    return result;
}

QString PlaylistImporter::makeMatchKey(const QString& artist, const QString& title)
{
    return artist.toLower() + QStringLiteral("|||") + title.toLower();
}
