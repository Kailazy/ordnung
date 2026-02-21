#include "WaveformGenerator.h"

#include <QProcess>
#include <QFileInfo>
#include <QStandardPaths>
#include <QtConcurrent>
#include <QDebug>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>

// ── Constructor ─────────────────────────────────────────────────────────────

WaveformGenerator::WaveformGenerator(QObject* parent)
    : QObject(parent)
{
}

// ── Public: single-track async generation ───────────────────────────────────

void WaveformGenerator::generate(const Track& track)
{
    const long long songId = track.id;
    const QString filepath = QString::fromStdString(track.filepath);

    auto* watcher = new QFutureWatcher<QByteArray>(this);

    connect(watcher, &QFutureWatcher<QByteArray>::finished, this,
            [this, watcher, songId]() {
                const QByteArray peaks = watcher->result();
                emit waveformReady(songId, peaks);
                watcher->deleteLater();
            });

    QFuture<QByteArray> future = QtConcurrent::run([filepath]() {
        return computePeaks(filepath);
    });

    watcher->setFuture(future);
}

// ── Public: batch generation (asynchronous) ─────────────────────────────────

void WaveformGenerator::generateBatch(const QVector<Track>& tracks)
{
    m_cancelled.store(false);

    (void)QtConcurrent::run([this, tracks]() {
        const int total = tracks.size();

        for (int i = 0; i < total; ++i) {
            if (m_cancelled.load())
                break;

            const Track& t = tracks[i];
            const QString fp = QString::fromStdString(t.filepath);

            const QByteArray peaks = computePeaks(fp);
            emit waveformReady(t.id, peaks);
            emit progress(i + 1, total);
        }

        emit finished();
    });
}

// ── Public: cancel ──────────────────────────────────────────────────────────

void WaveformGenerator::cancel()
{
    m_cancelled.store(true);
}

// ── Static: synchronous peak computation ────────────────────────────────────

QByteArray WaveformGenerator::computePeaks(const QString& filepath, int binCount)
{
    if (!QFileInfo::exists(filepath)) {
        qWarning() << "WaveformGenerator: file not found:" << filepath;
        return {};
    }

    // Locate ffmpeg binary
    const QString ffmpeg = QStandardPaths::findExecutable(QStringLiteral("ffmpeg"));
    if (ffmpeg.isEmpty()) {
        qWarning() << "WaveformGenerator: ffmpeg not found in PATH";
        return {};
    }

    // Decode to raw PCM: mono, 22050 Hz, signed 16-bit little-endian, piped to stdout.
    QProcess proc;
    proc.setProgram(ffmpeg);
    proc.setArguments({
        QStringLiteral("-i"), filepath,
        QStringLiteral("-vn"),
        QStringLiteral("-ac"), QStringLiteral("1"),
        QStringLiteral("-ar"), QStringLiteral("22050"),
        QStringLiteral("-f"), QStringLiteral("s16le"),
        QStringLiteral("-")
    });

    // Suppress ffmpeg banner on stderr so it doesn't fill the pipe buffer.
    proc.setProcessChannelMode(QProcess::SeparateChannels);
    proc.start();

    if (!proc.waitForFinished(120000)) {
        qWarning() << "WaveformGenerator: ffmpeg timed out for" << filepath;
        proc.kill();
        proc.waitForFinished(5000);
        return {};
    }

    if (proc.exitCode() != 0) {
        qWarning() << "WaveformGenerator: ffmpeg exited with code"
                   << proc.exitCode() << "for" << filepath;
        return {};
    }

    const QByteArray raw = proc.readAllStandardOutput();
    const int totalBytes = raw.size();
    const int sampleCount = totalBytes / static_cast<int>(sizeof(int16_t));

    if (sampleCount == 0) {
        qWarning() << "WaveformGenerator: ffmpeg returned 0 samples for" << filepath;
        return {};
    }

    // Interpret raw bytes as int16_t samples (little-endian on all supported platforms).
    const auto* samples = reinterpret_cast<const int16_t*>(raw.constData());

    // Compute per-bin peak absolute amplitude.
    QVector<int> binPeaks(binCount, 0);
    const int samplesPerBin = std::max(1, sampleCount / binCount);

    for (int bin = 0; bin < binCount; ++bin) {
        const int start = bin * samplesPerBin;
        const int end   = (bin == binCount - 1)
                              ? sampleCount
                              : std::min(start + samplesPerBin, sampleCount);

        int peak = 0;
        for (int s = start; s < end; ++s) {
            const int absVal = std::abs(static_cast<int>(samples[s]));
            if (absVal > peak)
                peak = absVal;
        }
        binPeaks[bin] = peak;
    }

    // Find global max for normalization.
    int globalMax = 0;
    for (int p : binPeaks) {
        if (p > globalMax)
            globalMax = p;
    }

    // Normalize to 0-255 and pack into QByteArray.
    QByteArray result(binCount, '\0');

    if (globalMax > 0) {
        auto* dst = reinterpret_cast<uint8_t*>(result.data());
        for (int i = 0; i < binCount; ++i) {
            dst[i] = static_cast<uint8_t>(
                (static_cast<long long>(binPeaks[i]) * 255) / globalMax
            );
        }
    }

    return result;
}
