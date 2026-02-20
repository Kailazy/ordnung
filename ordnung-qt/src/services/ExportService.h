#pragma once

#include <QObject>
#include <QString>
#include <QVector>
#include <atomic>

class Database;
struct Track;
struct CuePoint;
struct Playlist;

// Options controlling a Rekordbox XML or CDJ USB export.
struct ExportOptions {
    enum Target { RekordboxXml, CdjUsb };
    Target              target       = RekordboxXml;
    QString             outputPath;       // file path (XML) or USB mount dir
    QVector<long long>  playlistIds;      // empty = all playlists
    enum OutputFormat { KeepOriginal, ConvertToAiff, ConvertToWav };
    OutputFormat        outputFormat = KeepOriginal;
    bool                copyFiles   = true;  // USB only: copy audio files to USB
};

// Progress snapshot emitted per-track during export.
struct ExportProgress {
    int     done        = 0;
    int     total       = 0;
    QString currentFile;
};

// Generates Rekordbox-compatible XML and Pioneer CDJ USB folder structures.
// All heavy work runs off the main thread via QtConcurrent; connect to
// progress() and finished() signals for UI updates.
class ExportService : public QObject
{
    Q_OBJECT
public:
    explicit ExportService(Database* db, QObject* parent = nullptr);

    // Start an asynchronous export. Emits progress() per track, then finished().
    void startExport(const ExportOptions& opts);

    // Request cancellation of the running export (best-effort).
    void cancel();

signals:
    // Emitted once per track written into the XML / copied to USB.
    void progress(ExportProgress p);

    // Emitted when the export completes or fails. errorMsg is empty on success.
    void finished(bool success, const QString& errorMsg);

private:
    // Write a full rekordbox.xml to the given path. Returns empty string on success.
    QString writeRekordboxXml(const QString& xmlPath,
                              const QVector<Playlist>& playlists,
                              const QMap<long long, QVector<Track>>& playlistTracks,
                              const QVector<Track>& allTracks,
                              const QMap<long long, QVector<CuePoint>>& cueMap);

    // Create the PIONEER folder structure on a USB mount point.
    QString writeCdjUsb(const ExportOptions& opts,
                        const QVector<Playlist>& playlists,
                        const QMap<long long, QVector<Track>>& playlistTracks,
                        const QVector<Track>& allTracks,
                        const QMap<long long, QVector<CuePoint>>& cueMap);

    // Write a minimal ANLZ stub file (DAT or EXT) that CDJs accept.
    static bool writeAnlzStub(const QString& path);

    // Convert a 0-5 star rating to the Rekordbox 0-255 scale.
    static int ratingToRekordbox(int stars);

    // Map Pioneer color index (1-8) to RGB triplet for POSITION_MARK.
    static void pioneerColorToRgb(int colorIndex, int& r, int& g, int& b);

    Database*          m_db       = nullptr;
    std::atomic<bool>  m_cancelled{false};
};
