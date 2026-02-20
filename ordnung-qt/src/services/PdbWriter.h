#pragma once

#include <QString>
#include <QVector>
#include <QMap>

struct Playlist;
struct Track;
struct CuePoint;

/// Writes a Pioneer CDJ export.pdb binary database file.
///
/// The PDB format is documented by Deep Symmetry:
///   https://djl-analysis.deepsymmetry.org/rekordbox-export-analysis/exports.html
///   Kaitai struct: rekordbox_pdb.ksy
///
/// CDJ players read this file from PIONEER/rekordbox/export.pdb on a USB stick
/// to display playlists and tracks without requiring Rekordbox.
class PdbWriter {
public:
    /// Write export.pdb to path.
    /// Returns empty string on success, error message on failure.
    static QString write(const QString& path,
                         const QVector<Playlist>& playlists,
                         const QMap<long long, QVector<Track>>& playlistTracks,
                         const QVector<Track>& allTracks,
                         const QMap<long long, QVector<CuePoint>>& cueMap);

private:
    PdbWriter() = default;
};
