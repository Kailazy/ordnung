#pragma once

#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QVariantMap>
#include <QVector>

#include "services/Database.h"

// TrackDetailPanel displays expanded metadata for the selected track.
// Shown below the TrackTableView when a row is selected.
// Hidden by default (setVisible(false)).
class TrackDetailPanel : public QWidget
{
    Q_OBJECT
public:
    explicit TrackDetailPanel(Database* db, QWidget* parent = nullptr);

    // Populate the panel with data for the given track (as QVariantMap from RawTrackRole).
    void populate(const QVariantMap& trackData, long long activePlaylistId);

    // Clear and hide the panel.
    void clear();

signals:
    void aiffToggled(long long songId, bool newValue);
    void playlistMembershipChanged(long long songId, long long playlistId, bool added);

private slots:
    void onAiffToggled();
    void onPlaylistChipToggled(long long songId, long long playlistId, bool checked);

private:
    void buildPlaylistChips(long long songId, long long activePlaylistId);
    static QLabel* makeFieldLabel(const QString& key, QWidget* parent);
    static QLabel* makeValueLabel(const QString& value, QWidget* parent);
    static QString starsString(int rating);

    Database* m_db;

    long long  m_songId     = -1;
    bool       m_hasAiff    = false;

    // Left column — metadata grid
    QLabel* m_albumKey   = nullptr;
    QLabel* m_albumVal   = nullptr;
    QLabel* m_genreKey   = nullptr;
    QLabel* m_genreVal   = nullptr;
    QLabel* m_bpmKey     = nullptr;
    QLabel* m_bpmVal     = nullptr;
    QLabel* m_keyKey     = nullptr;
    QLabel* m_keyVal     = nullptr;
    QLabel* m_timeKey    = nullptr;
    QLabel* m_timeVal    = nullptr;
    QLabel* m_ratingKey  = nullptr;
    QLabel* m_ratingVal  = nullptr;
    QLabel* m_addedKey   = nullptr;
    QLabel* m_addedVal   = nullptr;

    // Right column — AIFF toggle + playlist chips
    QPushButton* m_aiffBtn      = nullptr;
    QWidget*     m_chipsWidget  = nullptr;
    QScrollArea* m_chipsScroll  = nullptr;
};
