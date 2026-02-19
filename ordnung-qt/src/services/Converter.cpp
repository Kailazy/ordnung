#include "Converter.h"

#include <QProcess>
#include <QFileInfo>
#include <QDir>
#include <QDateTime>
#include <QDebug>
#include <QMutexLocker>

ConversionWorker::ConversionWorker(Database* db, QObject* parent)
    : QObject(parent)
    , m_db(db)
{}

int ConversionWorker::queueSize() const
{
    QMutexLocker lock(const_cast<QMutex*>(&m_mutex));
    return m_queue.size() + (m_busy ? 1 : 0);
}

void ConversionWorker::enqueue(long long downloadId, const QString& sourcePath,
                                const QString& outputFolder)
{
    {
        QMutexLocker lock(&m_mutex);
        m_queue.enqueue({downloadId, sourcePath, outputFolder});
    }

    emit logLine(QStringLiteral("[%1]  Queued: %2")
                 .arg(QDateTime::currentDateTime().toString(QStringLiteral("hh:mm:ss")))
                 .arg(QFileInfo(sourcePath).fileName()));

    emit queueChanged(queueSize());

    if (!m_busy)
        processNext();
}

void ConversionWorker::processNext()
{
    QueuedJob job;
    {
        QMutexLocker lock(&m_mutex);
        if (m_queue.isEmpty()) {
            m_busy = false;
            emit queueChanged(0);
            return;
        }
        m_busy = true;
        job = m_queue.dequeue();
    }

    emit queueChanged(queueSize());

    const QString outputPath = buildOutputPath(job.sourcePath, job.outputFolder);
    const QString now = QDateTime::currentDateTime().toString(Qt::ISODate);

    // Record in DB
    const QString sourceExt = QFileInfo(job.sourcePath).suffix().toLower();
    const QFileInfo srcInfo(job.sourcePath);
    const double sizeMb = srcInfo.exists() ? static_cast<double>(srcInfo.size()) / (1024.0 * 1024.0) : 0.0;

    long long convId = m_db->insertConversion(
        job.downloadId, job.sourcePath, outputPath, sourceExt, sizeMb, now);

    if (convId < 0) {
        emit logLine(QStringLiteral("[%1]  ERROR: DB insert failed for %2")
                     .arg(now).arg(QFileInfo(job.sourcePath).fileName()));
        QMetaObject::invokeMethod(this, &ConversionWorker::processNext, Qt::QueuedConnection);
        return;
    }

    m_db->updateConversionStatus(convId, QStringLiteral("converting"));
    emit conversionStarted(convId, job.downloadId);

    emit logLine(QStringLiteral("[%1]  Converting: %2")
                 .arg(QDateTime::currentDateTime().toString(QStringLiteral("hh:mm:ss")))
                 .arg(QFileInfo(job.sourcePath).fileName()));

    // Ensure output directory exists
    QDir().mkpath(QFileInfo(outputPath).absolutePath());

    // Build ffmpeg arguments
    QStringList args;
    args << QStringLiteral("-y")          // overwrite without asking
         << QStringLiteral("-i") << job.sourcePath
         << QStringLiteral("-acodec") << QStringLiteral("pcm_s16be")  // AIFF standard codec
         << QStringLiteral("-ar") << QStringLiteral("44100")
         << outputPath;

    QProcess proc;
    proc.setProgram(QStringLiteral("ffmpeg"));
    proc.setArguments(args);
    proc.start();

    bool started = proc.waitForStarted(5000);
    if (!started) {
        const QString errMsg = QStringLiteral("ffmpeg not found or could not start");
        const QString finAt  = QDateTime::currentDateTime().toString(Qt::ISODate);
        m_db->updateConversionStatus(convId, QStringLiteral("failed"), errMsg, finAt);

        emit logLine(QStringLiteral("[%1]  ERROR: %2")
                     .arg(QDateTime::currentDateTime().toString(QStringLiteral("hh:mm:ss")))
                     .arg(errMsg));
        emit conversionFinished(convId, job.downloadId, false, errMsg);

        QMetaObject::invokeMethod(this, &ConversionWorker::processNext, Qt::QueuedConnection);
        return;
    }

    // Block on this thread — this IS the worker thread, blocking is intentional.
    proc.waitForFinished(-1);

    const bool success  = (proc.exitStatus() == QProcess::NormalExit && proc.exitCode() == 0);
    const QString finAt = QDateTime::currentDateTime().toString(Qt::ISODate);
    QString errMsg;

    if (!success) {
        errMsg = QString::fromLocal8Bit(proc.readAllStandardError()).trimmed();
        if (errMsg.size() > 300)
            errMsg = errMsg.left(300) + QStringLiteral("...");
    }

    m_db->updateConversionStatus(convId,
                                  success ? QStringLiteral("done") : QStringLiteral("failed"),
                                  errMsg, finAt);

    if (success) {
        emit logLine(QStringLiteral("[%1]  Done: %2")
                     .arg(QDateTime::currentDateTime().toString(QStringLiteral("hh:mm:ss")))
                     .arg(QFileInfo(outputPath).fileName()));
    } else {
        emit logLine(QStringLiteral("[%1]  ERROR converting %2: %3")
                     .arg(QDateTime::currentDateTime().toString(QStringLiteral("hh:mm:ss")))
                     .arg(QFileInfo(job.sourcePath).fileName())
                     .arg(errMsg));
    }

    emit conversionFinished(convId, job.downloadId, success, errMsg);

    // Continue processing queue
    QMetaObject::invokeMethod(this, &ConversionWorker::processNext, Qt::QueuedConnection);
}

QString ConversionWorker::buildOutputPath(const QString& sourcePath, const QString& outputFolder)
{
    const QFileInfo fi(sourcePath);
    QString base = fi.completeBaseName();
    QString outDir = outputFolder.isEmpty() ? fi.absolutePath() : outputFolder;
    return QDir(outDir).filePath(base + QStringLiteral(".aiff"));
}
