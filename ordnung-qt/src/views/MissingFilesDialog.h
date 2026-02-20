#pragma once

#include <QDialog>

class Database;
class QTableWidget;
class QLabel;

class MissingFilesDialog : public QDialog
{
    Q_OBJECT
public:
    explicit MissingFilesDialog(Database* db, QWidget* parent = nullptr);

signals:
    void libraryChanged();

private slots:
    void onLocateClicked(long long songId);
    void onRemoveClicked(long long songId);
    void onRemoveAllClicked();

private:
    void reload();

    Database*     m_db;
    QTableWidget* m_table      = nullptr;
    QLabel*       m_countLabel = nullptr;
};
