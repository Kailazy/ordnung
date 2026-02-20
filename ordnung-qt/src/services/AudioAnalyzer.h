#pragma once

#include <QObject>
#include <QString>
#include <QVector>
#include <atomic>

#include "core/Track.h"

// Register Track for cross-thread signal/slot delivery via queued connections.
Q_DECLARE_METATYPE(Track)

// Result of analyzing a single audio file with ffprobe (and optionally aubio/Essentia).
struct AnalysisResult {
    bool    success  = false;
    double  bpm      = 0.0;
    QString key;            // e.g. "Am", "C#m", "Bb"
    int     bitrate  = 0;   // kbps
    QString duration;       // "M:SS"
    QString error;

    // Essentia fields (only populated when Essentia is available)
    QString moodTags;
    QString styleTags;
    float   danceability = 0.0f;
    float   valence      = 0.0f;
    float   vocalProb    = 0.0f;
    bool    essentiaUsed = false;
};

// Extracts BPM, key, bitrate, and duration from audio files using ffprobe.
// Falls back to aubiotempo for BPM when metadata is missing.
// Batch analysis runs off the main thread; connect to progress() and finished().
class AudioAnalyzer : public QObject
{
    Q_OBJECT
public:
    explicit AudioAnalyzer(QObject* parent = nullptr);

    // Analyze a single file synchronously. Safe to call from any thread.
    static AnalysisResult analyzeFile(const QString& filepath);

    // Analyze a batch of tracks asynchronously. Emits progress per file.
    void analyzeLibrary(const QVector<Track>& tracks);

    // Request cancellation of the running batch analysis.
    void cancel();

signals:
    // Emitted after each individual file is analyzed with its updated metadata.
    // Connect to TrackModel::updateTrackMetadata() for incremental row updates.
    void trackAnalyzed(const Track& updated);

    // Emitted after each file is analyzed (progress indicator).
    void progress(int done, int total, const QString& currentFile);

    // Emitted when the batch is complete. updatedTracks has bpm/key/bitrate filled.
    void finished(const QVector<Track>& updatedTracks);

private:
    // Run ffprobe and parse JSON output for a single file.
    static AnalysisResult runFfprobe(const QString& filepath);

    // Run aubiotempo to detect BPM when metadata lacks it.
    static double runAubiotempo(const QString& filepath);

    // Format seconds as "M:SS".
    static QString formatDuration(double seconds);

    std::atomic<bool> m_cancelled{false};
};
