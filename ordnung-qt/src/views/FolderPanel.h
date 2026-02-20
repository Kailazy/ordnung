#pragma once

#include <QWidget>
#include <QLabel>
#include <QPushButton>

// FolderPanel — left sidebar in the Library view.
// Shows the current library folder path and lets the user pick a new one.
class FolderPanel : public QWidget
{
    Q_OBJECT
public:
    explicit FolderPanel(QWidget* parent = nullptr);

    void setFolder(const QString& path);
    QString folder() const { return m_folder; }

signals:
    void folderChanged(const QString& path);

private slots:
    void onBrowseClicked();

private:
    QString      m_folder;
    QLabel*      m_pathLabel  = nullptr;
    QPushButton* m_browseBtn  = nullptr;
};
