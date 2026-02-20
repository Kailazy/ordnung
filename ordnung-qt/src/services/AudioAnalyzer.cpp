#include "AudioAnalyzer.h"

#ifdef HAVE_ESSENTIA
#include "services/EssentiaAnalyzer.h"
#endif

#include <QProcess>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFileInfo>
#include <QStandardPaths>
#include <QtConcurrent>
#include <QDebug>

// ── Constructor ─────────────────────────────────────────────────────────────

AudioAnalyzer::AudioAnalyzer(QObject* parent)
    : QObject(parent)
{
    // Register for queued cross-thread signal delivery
    qRegisterMetaType<Track>("Track");
}

// ── Public: single-file analysis (synchronous, call from worker thread) ─────

AnalysisResult AudioAnalyzer::analyzeFile(const QString& filepath)
{
    if (!QFileInfo::exists(filepath))
        return AnalysisResult{false, 0.0, {}, 0, {}, QStringLiteral("File not found: ") + filepath};

#ifdef HAVE_ESSENTIA
    // Prefer Essentia deep analysis when available
    if (EssentiaAnalyzer::isAvailable()) {
        AnalysisResult result = EssentiaAnalyzer::analyze(filepath);
        if (result.success) {
            // Essentia gives us BPM and key; we still need ffprobe for bitrate/duration
            AnalysisResult probe = runFfprobe(filepath);
            if (probe.success) {
                result.bitrate  = probe.bitrate;
                result.duration = probe.duration;
            }
            return result;
        }
        // If Essentia failed for this file, fall through to ffprobe+aubio
        qWarning() << "EssentiaAnalyzer failed for" << filepath
                   << "- falling back to ffprobe:" << result.error;
    }
#endif

    // Fallback: ffprobe + aubiotempo pipeline
    AnalysisResult result = runFfprobe(filepath);
    if (!result.success)
        return result;

    // Step 2: if BPM is still 0, try aubiotempo
    if (result.bpm <= 0.0) {
        const double aubioBpm = runAubiotempo(filepath);
        if (aubioBpm > 0.0)
            result.bpm = aubioBpm;
    }

    return result;
}

// ── Public: batch analysis (asynchronous) ───────────────────────────────────

void AudioAnalyzer::analyzeLibrary(const QVector<Track>& tracks)
{
    m_cancelled.store(false);

    (void)QtConcurrent::run([this, tracks]() {
        QVector<Track> updated;
        updated.reserve(tracks.size());
        const int total = tracks.size();

        for (int i = 0; i < total; ++i) {
            if (m_cancelled.load())
                break;

            Track t = tracks[i];
            const QString fp = QString::fromStdString(t.filepath);

            emit progress(i, total, QFileInfo(fp).fileName());

            AnalysisResult ar = analyzeFile(fp);
            if (ar.success) {
                if (ar.bpm > 0.0)
                    t.bpm = ar.bpm;
                if (!ar.key.isEmpty())
                    t.key_sig = ar.key.toStdString();
                if (ar.bitrate > 0)
                    t.bitrate = ar.bitrate;
                if (!ar.duration.isEmpty())
                    t.time = ar.duration.toStdString();

                // Essentia deep analysis fields
                if (ar.essentiaUsed) {
                    t.mood_tags          = ar.moodTags.toStdString();
                    t.style_tags         = ar.styleTags.toStdString();
                    t.danceability       = ar.danceability;
                    t.valence            = ar.valence;
                    t.vocal_prob         = ar.vocalProb;
                    t.essentia_analyzed  = true;
                }
            } else {
                qWarning() << "AudioAnalyzer: failed for" << fp << ":" << ar.error;
            }

            emit trackAnalyzed(t);
            updated.append(t);
        }

        // Final progress tick
        emit progress(total, total, QString());
        emit finished(updated);
    });
}

void AudioAnalyzer::cancel()
{
    m_cancelled.store(true);
}

// ── Private: ffprobe ────────────────────────────────────────────────────────

AnalysisResult AudioAnalyzer::runFfprobe(const QString& filepath)
{
    AnalysisResult result;

    // Locate ffprobe binary
    const QString ffprobe = QStandardPaths::findExecutable(QStringLiteral("ffprobe"));
    if (ffprobe.isEmpty()) {
        result.error = QStringLiteral("ffprobe not found in PATH");
        return result;
    }

    QProcess proc;
    proc.setProgram(ffprobe);
    proc.setArguments({
        QStringLiteral("-v"), QStringLiteral("quiet"),
        QStringLiteral("-print_format"), QStringLiteral("json"),
        QStringLiteral("-show_format"),
        QStringLiteral("-show_streams"),
        filepath
    });
    proc.start();

    if (!proc.waitForFinished(30000)) {
        result.error = QStringLiteral("ffprobe timed out");
        proc.kill();
        return result;
    }
    if (proc.exitCode() != 0) {
        result.error = QStringLiteral("ffprobe exited with code ")
                       + QString::number(proc.exitCode());
        return result;
    }

    const QByteArray output = proc.readAllStandardOutput();
    QJsonParseError parseErr;
    const QJsonDocument doc = QJsonDocument::fromJson(output, &parseErr);
    if (parseErr.error != QJsonParseError::NoError) {
        result.error = QStringLiteral("ffprobe JSON parse error: ") + parseErr.errorString();
        return result;
    }

    const QJsonObject root = doc.object();
    const QJsonObject format = root.value(QStringLiteral("format")).toObject();
    const QJsonArray  streams = root.value(QStringLiteral("streams")).toArray();

    // Duration from format section -> "M:SS"
    const double durationSec = format.value(QStringLiteral("duration")).toString().toDouble();
    if (durationSec > 0.0)
        result.duration = formatDuration(durationSec);

    // Bitrate: prefer the first audio stream's bit_rate, fall back to format bit_rate
    int bitrate = 0;
    for (const auto& s : streams) {
        const QJsonObject stream = s.toObject();
        if (stream.value(QStringLiteral("codec_type")).toString() == QLatin1String("audio")) {
            bitrate = stream.value(QStringLiteral("bit_rate")).toString().toInt() / 1000;
            break;
        }
    }
    if (bitrate <= 0)
        bitrate = format.value(QStringLiteral("bit_rate")).toString().toInt() / 1000;
    result.bitrate = bitrate;

    // Tags: look for BPM and key in format.tags (case-insensitive search)
    const QJsonObject tags = format.value(QStringLiteral("tags")).toObject();
    for (auto it = tags.constBegin(); it != tags.constEnd(); ++it) {
        const QString key = it.key().toLower();
        if (key == QLatin1String("bpm") || key == QLatin1String("tbpm")) {
            const double bpm = it.value().toString().toDouble();
            if (bpm > 0.0)
                result.bpm = bpm;
        } else if (key == QLatin1String("key") || key == QLatin1String("initial_key")
                   || key == QLatin1String("initialkey")) {
            const QString val = it.value().toString().trimmed();
            if (!val.isEmpty())
                result.key = val;
        }
    }

    // Also check stream-level tags (some formats embed BPM there)
    for (const auto& s : streams) {
        const QJsonObject stream = s.toObject();
        if (stream.value(QStringLiteral("codec_type")).toString() != QLatin1String("audio"))
            continue;
        const QJsonObject stags = stream.value(QStringLiteral("tags")).toObject();
        for (auto it = stags.constBegin(); it != stags.constEnd(); ++it) {
            const QString k = it.key().toLower();
            if ((k == QLatin1String("bpm") || k == QLatin1String("tbpm")) && result.bpm <= 0.0)
                result.bpm = it.value().toString().toDouble();
            if ((k == QLatin1String("key") || k == QLatin1String("initial_key")
                 || k == QLatin1String("initialkey")) && result.key.isEmpty())
                result.key = it.value().toString().trimmed();
        }
        break;  // only check the first audio stream
    }

    result.success = true;
    return result;
}

// ── Private: aubiotempo fallback ────────────────────────────────────────────

double AudioAnalyzer::runAubiotempo(const QString& filepath)
{
    const QString aubio = QStandardPaths::findExecutable(QStringLiteral("aubiotempo"));
    if (aubio.isEmpty())
        return 0.0;

    QProcess proc;
    proc.setProgram(aubio);
    proc.setArguments({QStringLiteral("-i"), filepath});
    proc.start();

    if (!proc.waitForFinished(60000)) {
        proc.kill();
        return 0.0;
    }
    if (proc.exitCode() != 0)
        return 0.0;

    // aubiotempo prints one BPM value per line; take the last non-empty line.
    const QByteArray output = proc.readAllStandardOutput();
    const QList<QByteArray> lines = output.split('\n');
    double lastBpm = 0.0;
    for (auto it = lines.crbegin(); it != lines.crend(); ++it) {
        const QByteArray line = it->trimmed();
        if (line.isEmpty())
            continue;
        bool ok = false;
        const double val = line.toDouble(&ok);
        if (ok && val > 0.0) {
            lastBpm = val;
            break;
        }
    }
    return lastBpm;
}

// ── Private: duration formatting ────────────────────────────────────────────

QString AudioAnalyzer::formatDuration(double seconds)
{
    const int total = static_cast<int>(seconds + 0.5);  // round to nearest second
    const int m = total / 60;
    const int s = total % 60;
    return QStringLiteral("%1:%2").arg(m).arg(s, 2, 10, QLatin1Char('0'));
}
