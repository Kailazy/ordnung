#include "FolderWatcher.h"
#include "Database.h"

#include <QDir>
#include <QFileInfo>
#include <QDateTime>
#include <QDebug>

static const QStringList kAudioExtensions = {
    QStringLiteral("mp3"),  QStringLiteral("flac"), QStringLiteral("wav"),
    QStringLiteral("aiff"), QStringLiteral("aif"),  QStringLiteral("m4a"),
    QStringLiteral("alac"), QStringLiteral("ogg"),  QStringLiteral("wma"),
    QStringLiteral("aac"),  QStringLiteral("opus")
};

FolderWatcher::FolderWatcher(Database* db, QObject* parent)
    : QObject(parent)
    , m_db(db)
{
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged,
            this, &FolderWatcher::onDirectoryChanged);
    connect(&m_watcher, &QFileSystemWatcher::fileChanged,
            this, &FolderWatcher::onFileChanged);
}

void FolderWatcher::setFolder(const QString& folderPath)
{
    if (!m_currentFolder.isEmpty() && m_watcher.directories().contains(m_currentFolder))
        m_watcher.removePath(m_currentFolder);

    m_currentFolder = folderPath;

    if (!folderPath.isEmpty() && QDir(folderPath).exists()) {
        m_watcher.addPath(folderPath);
        emit logLine(QStringLiteral("[%1]  Watching folder: %2")
                     .arg(QDateTime::currentDateTime().toString(QStringLiteral("hh:mm:ss")))
                     .arg(folderPath));
    }
}

FolderWatcher::ScanResult FolderWatcher::scan(const QString& folderPath,
                                               const QString& detectedAt)
{
    ScanResult result;

    QDir dir(folderPath);
    if (!dir.exists()) return result;

    const QFileInfoList entries = dir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot);
    for (const QFileInfo& fi : entries) {
        const QString ext = fi.suffix().toLower();
        if (!kAudioExtensions.contains(ext)) continue;

        ++result.scanned;

        const QString fp = fi.absoluteFilePath();
        if (m_db->downloadExists(fp)) continue;

        const double sizeMb = static_cast<double>(fi.size()) / (1024.0 * 1024.0);
        const long long id  = m_db->insertDownload(
            fi.fileName(), fp, ext, sizeMb, detectedAt);

        if (id > 0) ++result.added;
    }
    return result;
}

void FolderWatcher::onDirectoryChanged(const QString& path)
{
    // A file was added or removed in the watched folder.
    QDir dir(path);
    const QFileInfoList entries = dir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot);
    for (const QFileInfo& fi : entries) {
        const QString ext = fi.suffix().toLower();
        if (!kAudioExtensions.contains(ext)) continue;

        const QString fp = fi.absoluteFilePath();
        if (!m_db->downloadExists(fp)) {
            const double sizeMb = static_cast<double>(fi.size()) / (1024.0 * 1024.0);
            const QString now = QDateTime::currentDateTime().toString(Qt::ISODate);
            m_db->insertDownload(fi.fileName(), fp, ext, sizeMb, now);
            emit fileDetected(fp);
            emit logLine(QStringLiteral("[%1]  New file detected: %2")
                         .arg(QDateTime::currentDateTime().toString(QStringLiteral("hh:mm:ss")))
                         .arg(fi.fileName()));
        }
    }
}

void FolderWatcher::onFileChanged(const QString& /*path*/)
{
    // File change in watched paths — not typically used for the source folder.
}

QStringList FolderWatcher::audioExtensions()
{
    return kAudioExtensions;
}

bool FolderWatcher::isAudioFile(const QString& path)
{
    return kAudioExtensions.contains(QFileInfo(path).suffix().toLower());
}
