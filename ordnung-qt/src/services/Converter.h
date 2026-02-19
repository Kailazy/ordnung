#pragma once

#include <QObject>
#include <QString>
#include <QQueue>
#include <QMutex>

#include "core/ConversionJob.h"
#include "services/Database.h"

// ConversionWorker is a QObject that must be moved to a QThread.
// Never subclass QThread — worker-object pattern only.
//
// Callers dispatch work via QMetaObject::invokeMethod with Qt::QueuedConnection.
class ConversionWorker : public QObject
{
    Q_OBJECT
public:
    explicit ConversionWorker(Database* db, QObject* parent = nullptr);

    int queueSize() const;

public slots:
    // Enqueue a conversion. outputFolder is where the .aiff goes.
    void enqueue(long long downloadId, const QString& sourcePath,
                 const QString& outputFolder);

    // Process the next job in the queue (called internally after each completion).
    void processNext();

signals:
    void conversionStarted(long long convId, long long downloadId);
    void conversionFinished(long long convId, long long downloadId,
                            bool success, const QString& error);
    void logLine(const QString& line);
    void queueChanged(int size);

private:
    QString buildOutputPath(const QString& sourcePath, const QString& outputFolder);

    struct QueuedJob {
        long long downloadId;
        QString   sourcePath;
        QString   outputFolder;
    };

    Database*         m_db;
    QQueue<QueuedJob> m_queue;
    QMutex            m_mutex;
    bool              m_busy = false;
};
