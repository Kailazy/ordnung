#pragma once

#include <QDialog>
#include <QVector>

#include "services/Database.h"

class QTableWidget;
class QLabel;
class QPushButton;

// DuplicateDetectorDialog — scans the library for duplicate tracks
// (same artist+title) and presents them for review.
// User can remove one track from each pair or skip.
class DuplicateDetectorDialog : public QDialog
{
    Q_OBJECT
public:
    explicit DuplicateDetectorDialog(Database* db, QWidget* parent = nullptr);

    // Returns IDs of tracks that were removed during the session.
    QVector<long long> removedIds() const { return m_removedIds; }

private slots:
    void onRemoveA(int row);
    void onRemoveB(int row);
    void removeRow(int row, long long removeId);

private:
    void buildTable(const QVector<Database::DuplicatePair>& pairs);
    static QString formatTrackCell(const Track& t);

    Database*    m_db;
    QTableWidget* m_table    = nullptr;
    QLabel*       m_countLbl = nullptr;
    QPushButton*  m_closeBtn = nullptr;

    QVector<Database::DuplicatePair> m_pairs;
    QVector<long long>               m_removedIds;
};
