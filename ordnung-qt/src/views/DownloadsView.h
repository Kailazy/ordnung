#pragma once

#include <QWidget>
#include <QString>
#include <QVector>

#include "services/Database.h"
#include "core/ConversionJob.h"

class DownloadsModel;
class StatusDelegate;
class QTableView;
class QLabel;
class QPushButton;
class QPlainTextEdit;
class QTimer;
class QResizeEvent;

// FolderNode — a clickable rounded card displaying a watch folder path.
// Hover/press states handled via QSS (#folderNodeSrc / #folderNodeOut).
class FolderNode : public QWidget
{
    Q_OBJECT
public:
    // roleLabel: "SOURCE" or "AIFF OUTPUT". objectId: "folderNodeSrc" or "folderNodeOut".
    explicit FolderNode(const QString& objectId, const QString& roleLabel,
                        QWidget* parent = nullptr);

    void    setPath(const QString& path);
    QString path() const { return m_path; }

signals:
    void clicked();

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    void updatePathLabel();

    QString  m_path;
    QLabel*  m_pathLabel = nullptr;
};

// DownloadsView — the Downloads tab content.
class DownloadsView : public QWidget
{
    Q_OBJECT
public:
    explicit DownloadsView(DownloadsModel* model, QWidget* parent = nullptr);

    void setWatchConfig(const WatchConfig& cfg);
    WatchConfig currentConfig() const;

    void appendLogLine(const QString& line);
    void onConversionUpdate(long long downloadId, long long convId,
                            ConversionStatus status, const QString& error);
    void reloadTable();

signals:
    void saveConfigRequested(const WatchConfig& cfg);
    void scanRequested(const QString& folder);
    void convertAllRequested(const QString& outputFolder);
    void convertSingleRequested(long long downloadId, const QString& sourcePath,
                                const QString& outputFolder);
    void deleteDownloadRequested(long long id);

private slots:
    void onSrcNodeClicked();
    void onOutNodeClicked();
    void onSaveClicked();
    void onScanClicked();
    void onConvertAllClicked();
    void onConvertRequested(const QModelIndex& index);
    void onTableContextMenu(const QPoint& pos);
    void onDownloadsSectionResized(int logicalIndex, int oldSize, int newSize);

protected:
    void resizeEvent(QResizeEvent* event) override;

private:
    void triggerLogPulse();
    void applyDownloadsColumnWidths();

    DownloadsModel* m_model;
    StatusDelegate* m_delegate;

    FolderNode*  m_srcNode       = nullptr;
    FolderNode*  m_outNode       = nullptr;
    QPushButton* m_saveBtn       = nullptr;
    QPushButton* m_scanBtn       = nullptr;
    QPushButton* m_convertAllBtn = nullptr;
    QLabel*      m_busyLabel     = nullptr;

    QVector<int>    m_downloadsColumnWeights;
    QVector<int>    m_downloadsColumnMinWidths;
    QTableView*     m_tableView = nullptr;
    QPlainTextEdit* m_logEdit   = nullptr;
    QLabel*         m_logDot    = nullptr;
    QTimer*         m_logDotTimer = nullptr;
};
