#include "M3UExporter.h"

#include <QFile>
#include <QTextStream>
#include <QFileInfo>
#include <QDateTime>
#include <QDebug>

// ─────────────────────────────────────────────────────────────────────────────

static int durationSeconds(const std::string& timeStr)
{
    // Parse "M:SS" or "H:MM:SS" format stored in Track::time
    const QString t = QString::fromStdString(timeStr);
    const QStringList parts = t.split(QLatin1Char(':'));
    int total = 0;
    for (const QString& p : parts)
        total = total * 60 + p.toInt();
    return total;
}

// ─────────────────────────────────────────────────────────────────────────────

bool M3UExporter::exportTracks(const QVector<Track>& tracks,
                                const QString& outputPath,
                                const QString& playlistName)
{
    QFile file(outputPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        qWarning() << "M3UExporter: cannot open output file:" << outputPath
                   << file.errorString();
        return false;
    }

    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);

    // Header
    out << "#EXTM3U\n";
    if (!playlistName.isEmpty())
        out << "# Eyebags Terminal — " << playlistName << "\n";
    out << "# Exported: " << QDateTime::currentDateTime().toString(Qt::ISODate) << "\n";
    out << "# Tracks: " << tracks.size() << "\n\n";

    for (const Track& t : tracks) {
        if (t.filepath.empty()) continue;

        const QString filepath = QString::fromStdString(t.filepath);
        const QString title    = QString::fromStdString(t.title);
        const QString artist   = QString::fromStdString(t.artist);
        const int     duration = durationSeconds(t.time);

        // #EXTINF:<duration>,<artist> - <title>
        const QString displayName = artist.isEmpty()
            ? title
            : artist + QStringLiteral(" - ") + title;

        out << QStringLiteral("#EXTINF:%1,%2\n").arg(duration).arg(displayName);
        out << filepath << "\n";
    }

    file.close();
    qInfo() << "M3UExporter: wrote" << tracks.size() << "tracks to" << outputPath;
    return true;
}

int M3UExporter::exportToFile(const QVector<Track>& tracks, const QString& outputPath)
{
    const bool ok = exportTracks(tracks, outputPath);
    return ok ? tracks.size() : -1;
}
