#pragma once

#include <QString>
#include <QVector>

#include "core/Track.h"

// M3UExporter — writes an M3U Extended playlist file from a track list.
// Format: #EXTM3U header + per-track #EXTINF + file path lines.
// Paths are written as absolute file:/// URIs for maximum player compatibility.
class M3UExporter
{
public:
    // Export tracks to an M3U file at outputPath.
    // playlistName is used as a comment in the header.
    // Returns true on success, false on write failure.
    static bool exportTracks(const QVector<Track>& tracks,
                              const QString& outputPath,
                              const QString& playlistName = QString());

    // Export a single playlist's tracks (convenience overload using DB).
    // Returns the number of tracks written, or -1 on failure.
    static int exportToFile(const QVector<Track>& tracks,
                             const QString& outputPath);
};
