#pragma once

#ifdef HAVE_ESSENTIA

#include <QString>
#include "AudioAnalyzer.h"  // for AnalysisResult

// EssentiaAnalyzer — deep audio analysis using Essentia's BeatTrackerMultiFeature
// and KeyExtractor algorithms, plus Discogs-Effnet ONNX model for genre/mood/
// danceability/vocal classification.
//
// This is a static utility class; all methods are thread-safe and stateless.
// When Essentia or the ONNX model is not available, isAvailable() returns false
// and the caller should fall back to the ffprobe + aubiotempo pipeline.
class EssentiaAnalyzer
{
public:
    // Returns true if Essentia is properly initialized and the Discogs-Effnet
    // ONNX model file is found on disk.
    static bool isAvailable();

    // Runs BeatTrackerMultiFeature + KeyExtractor + Discogs-Effnet on a file.
    // Returns a fully populated AnalysisResult with essentiaUsed = true on success.
    // Bitrate and duration are NOT filled here (caller should run ffprobe for those).
    static AnalysisResult analyze(const QString& filepath);

private:
    // Search for the Discogs-Effnet ONNX model in standard locations.
    // Returns the full path if found, or an empty string.
    static QString findDiscogsModel();
};

#endif // HAVE_ESSENTIA
