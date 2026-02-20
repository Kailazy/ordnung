#pragma once

#include <QDialog>
#include <QVector>

#include "core/Track.h"

class Database;
class QLineEdit;
class QPushButton;
class QLabel;

class BatchEditDialog : public QDialog
{
    Q_OBJECT
public:
    explicit BatchEditDialog(const QVector<Track>& tracks, Database* db,
                             QWidget* parent = nullptr);

signals:
    void applied();

private slots:
    void onApplyClicked();
    void onCancelClicked();

private:
    void setupStarRow();

    QLineEdit* m_artistEdit  = nullptr;
    QLineEdit* m_albumEdit   = nullptr;
    QLineEdit* m_genreEdit   = nullptr;
    QLineEdit* m_bpmEdit     = nullptr;
    QLineEdit* m_commentEdit = nullptr;
    int        m_rating      = 0;  // 0 = unchanged
    QVector<QPushButton*> m_starBtns;

    QVector<Track> m_tracks;
    Database*      m_db;
};
