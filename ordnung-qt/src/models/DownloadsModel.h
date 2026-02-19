#pragma once

#include <QAbstractTableModel>
#include <QVector>

#include "core/ConversionJob.h"

class Database;

class DownloadsModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    enum Column {
        ColFilename  = 0,
        ColExt       = 1,
        ColSize      = 2,
        ColStatus    = 3,
        ColAction    = 4,
        ColCount
    };

    enum Role {
        DownloadIdRole  = Qt::UserRole,
        ConvIdRole      = Qt::UserRole + 1,
        ConvStatusRole  = Qt::UserRole + 2,
        FilePathRole    = Qt::UserRole + 3,
    };

    explicit DownloadsModel(Database* db, QObject* parent = nullptr);

    void reload();

    // Update a single row's conversion status (called after worker signals).
    void setConversionStatus(long long downloadId, long long convId, ConversionStatus status,
                             const QString& errorMsg = QString());

    int      rowCount(const QModelIndex& parent = {}) const override;
    int      columnCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;

    const Download* downloadAt(int row) const;
    void removeRow(long long downloadId);

private:
    Database*       m_db;
    QVector<Download> m_downloads;

    static QString statusLabel(ConversionStatus s);
};
