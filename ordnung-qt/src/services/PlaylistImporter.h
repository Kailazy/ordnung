#pragma once

#include <QObject>
#include <QString>
#include <QVector>

#include "core/Track.h"

struct ImportResult {
    QVector<Track> tracks;
    QString        error;
    bool           ok = false;
};

// Parses Rekordbox tab-separated .txt exports.
// Handles UTF-8, UTF-8-with-BOM, and UTF-16 LE (with BOM).
// The resulting Track objects have id=0 and match_key set.
// Callers must persist via Database::upsertSong().
class PlaylistImporter : public QObject
{
    Q_OBJECT
public:
    explicit PlaylistImporter(QObject* parent = nullptr);

    ImportResult parse(const QString& filePath);

    static QString makeMatchKey(const QString& artist, const QString& title);

private:
    Track parseLine(const QStringList& fields, const QStringList& headers);
};
