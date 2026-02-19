#pragma once

#include <QObject>
#include <QFileSystemWatcher>
#include <QString>
#include <QStringList>

class Database;

class FolderWatcher : public QObject
{
    Q_OBJECT
public:
    explicit FolderWatcher(Database* db, QObject* parent = nullptr);

    // Set (or update) the folder being watched.
    void setFolder(const QString& folderPath);

    // One-shot scan: finds all audio files in folderPath and inserts any
    // new ones into the downloads table. Returns {scanned, added} counts.
    struct ScanResult { int scanned = 0; int added = 0; };
    ScanResult scan(const QString& folderPath, const QString& detectedAt);

signals:
    // Emitted when a new audio file appears in the watched folder.
    void fileDetected(const QString& filePath);
    void logLine(const QString& line);

private slots:
    void onDirectoryChanged(const QString& path);
    void onFileChanged(const QString& path);

private:
    static QStringList audioExtensions();
    static bool isAudioFile(const QString& path);

    Database*          m_db;
    QFileSystemWatcher m_watcher;
    QString            m_currentFolder;
};
