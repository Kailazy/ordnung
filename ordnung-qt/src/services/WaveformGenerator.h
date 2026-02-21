#pragma once

#include <QObject>
#include <QByteArray>
#include <QString>
#include <QVector>
#include <QFutureWatcher>
#include <atomic>

#include "core/Track.h"

/// WaveformGenerator -- computes peak-amplitude waveform overview for an audio file.
/// Uses ffmpeg subprocess to decode audio to raw PCM, then computes per-bin
/// peak amplitude values normalized to uint8 (0-255).
///
/// Usage:
///   auto* gen = new WaveformGenerator(this);
///   connect(gen, &WaveformGenerator::waveformReady, ...);
///   gen->generate(track);
class WaveformGenerator : public QObject
{
    Q_OBJECT
public:
    explicit WaveformGenerator(QObject* parent = nullptr);

    /// Kick off asynchronous waveform generation for a single track.
    /// Emits waveformReady(songId, peaks) when done.
    void generate(const Track& track);

    /// Kick off asynchronous waveform generation for a batch of tracks.
    /// Emits waveformReady() per track, progress() after each, finished() at the end.
    void generateBatch(const QVector<Track>& tracks);

    /// Request cancellation of the running batch.
    void cancel();

    /// Synchronous peak computation. Safe to call from any thread.
    /// Returns a QByteArray of binCount uint8 values (0-255), one per bin.
    /// Returns an empty QByteArray on failure.
    static QByteArray computePeaks(const QString& filepath, int binCount = 800);

signals:
    /// Emitted when one track's waveform peaks are ready.
    /// peaks is empty if generation failed.
    void waveformReady(long long songId, QByteArray peaks);

    /// Batch progress: done out of total tracks completed.
    void progress(int done, int total);

    /// Emitted when the batch is complete (or cancelled).
    void finished();

private:
    std::atomic<bool> m_cancelled{false};
};
