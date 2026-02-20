#pragma once

#include <QVector>
#include <QString>
#include "core/Track.h"

// LibraryScanner — static utility to scan a folder tree for audio files.
// Returns Track objects with filepath, title, artist, and format populated.
// IDs are assigned sequentially starting from 1 (not DB-backed).
class LibraryScanner
{
public:
    // Full scan: extracts metadata via ffprobe (slow, one subprocess per file).
    static QVector<Track> scan(const QString& folder);

    // Fast scan: filename-based only, no ffprobe. Returns immediately.
    // Use this to populate the table quickly; follow up with AudioAnalyzer::analyzeLibrary().
    static QVector<Track> scanFast(const QString& folder);

private:
    static const QStringList& audioExtensions();
};
