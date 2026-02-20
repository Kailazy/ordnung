#pragma once

#include <QWidget>
#include <QVector>
#include "core/CuePoint.h"

class Database;

// ─────────────────────────────────────────────────────────────────────────────
// CuePad — one hot-cue slot button (A–H).
//
// Empty: muted slab with slot letter.
// Occupied: Pioneer colour fill, letter + truncated name.
// Single-click: create (if empty) or no-op (if set).
// Double-click: rename.
// Right-click: context menu with Delete.
// ─────────────────────────────────────────────────────────────────────────────
class CuePad : public QWidget
{
    Q_OBJECT
public:
    explicit CuePad(int slot, QWidget* parent = nullptr);

    // Update display. Pass nullptr to mark the pad as empty.
    void setCue(const CuePoint* cue);
    bool isOccupied() const { return m_occupied; }

signals:
    void createRequested(int slot);
    void deleteRequested(int slot);
    void renameRequested(int slot);

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseDoubleClickEvent(QMouseEvent*) override;
    void contextMenuEvent(QContextMenuEvent*) override;
    void enterEvent(QEnterEvent*) override;
    void leaveEvent(QEvent*) override;

private:
    int     m_slot;
    bool    m_occupied = false;
    QString m_name;
    int     m_color    = 1;   // Pioneer color index 1-8
    bool    m_hovered  = false;
};


// ─────────────────────────────────────────────────────────────────────────────
// CuePointEditor — row of 8 hot-cue pads backed by the DB.
// ─────────────────────────────────────────────────────────────────────────────
class CuePointEditor : public QWidget
{
    Q_OBJECT
public:
    explicit CuePointEditor(Database* db, QWidget* parent = nullptr);

    // Load and display cue points for the given song.
    void loadCues(long long songId);

    // Clear (call when detail panel is hidden).
    void clear();

private slots:
    void onCreateRequested(int slot);
    void onDeleteRequested(int slot);
    void onRenameRequested(int slot);

private:
    void refresh();

    Database*          m_db;
    long long          m_songId = -1;
    QVector<CuePoint>  m_cues;
    QVector<CuePad*>   m_pads;   // 8 pads, index = slot 0-7
};
