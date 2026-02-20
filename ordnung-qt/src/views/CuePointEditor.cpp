#include "CuePointEditor.h"
#include "services/Database.h"
#include "style/Theme.h"

#include <QPainter>
#include <QPainterPath>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QAction>
#include <QInputDialog>
#include <QMouseEvent>
#include <QContextMenuEvent>
#include <QFontMetrics>
#include <QApplication>

// ─────────────────────────────────────────────────────────────────────────────
// Constants
// ─────────────────────────────────────────────────────────────────────────────

static constexpr int kPadSize    = 52;   // px — square pad
static constexpr int kPadRadius  = 4;    // corner radius
static constexpr int kPadSpacing = 6;

static const char kSlotLetters[] = "ABCDEFGH";

// ─────────────────────────────────────────────────────────────────────────────
// CuePad
// ─────────────────────────────────────────────────────────────────────────────

CuePad::CuePad(int slot, QWidget* parent)
    : QWidget(parent)
    , m_slot(slot)
{
    setFixedSize(kPadSize, kPadSize);
    setCursor(Qt::PointingHandCursor);
    setMouseTracking(true);
}

void CuePad::setCue(const CuePoint* cue)
{
    m_occupied = (cue != nullptr);
    m_name     = cue ? QString::fromStdString(cue->name) : QString();
    m_color    = (cue && cue->color >= 1 && cue->color <= 8) ? cue->color : 1;
    update();
}

void CuePad::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::TextAntialiasing);

    const QRectF r = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    QPainterPath bg;
    bg.addRoundedRect(r, kPadRadius, kPadRadius);

    if (m_occupied) {
        // Filled with Pioneer palette color
        QColor c = Theme::Color::labelColor(m_color);
        if (m_hovered)
            c = c.lighter(120);
        p.fillPath(bg, c);
        p.setPen(Qt::NoPen);
    } else {
        // Empty: dark bg, dashed border
        QColor bgCol = m_hovered ? QColor(Theme::Color::Bg3) : QColor(Theme::Color::Bg2);
        p.fillPath(bg, bgCol);
        QPen pen(QColor(m_hovered ? Theme::Color::BorderHov : Theme::Color::Border), 1,
                 Qt::DashLine);
        p.strokePath(bg, pen);
    }

    // Slot letter (top-left)
    const QString letter = QString(QLatin1Char(kSlotLetters[qBound(0, m_slot, 7)]));
    QFont letterFont = QApplication::font();
    letterFont.setPointSize(Theme::Font::Caption);
    letterFont.setWeight(QFont::DemiBold);
    p.setFont(letterFont);
    p.setPen(m_occupied ? QColor(Qt::black).lighter(180) : QColor(Theme::Color::Text3));
    p.drawText(QRect(6, 4, 20, 16), Qt::AlignLeft | Qt::AlignVCenter, letter);

    // Name label (centre)
    if (m_occupied && !m_name.isEmpty()) {
        QFont nameFont = QApplication::font();
        nameFont.setPointSize(Theme::Font::Small);
        p.setFont(nameFont);
        p.setPen(Qt::black);
        const QFontMetrics fm(nameFont);
        const QRect textRect = QRect(4, 20, kPadSize - 8, kPadSize - 24);
        p.drawText(textRect, Qt::AlignHCenter | Qt::AlignVCenter | Qt::TextWordWrap,
                   fm.elidedText(m_name, Qt::ElideRight, textRect.width()));
    } else if (!m_occupied) {
        // Show "+" hint on hover
        if (m_hovered) {
            QFont hintFont = QApplication::font();
            hintFont.setPointSize(Theme::Font::Caption);
            p.setFont(hintFont);
            p.setPen(QColor(Theme::Color::Text3));
            p.drawText(QRect(0, 0, kPadSize, kPadSize),
                       Qt::AlignHCenter | Qt::AlignVCenter, QStringLiteral("+"));
        }
    }
}

void CuePad::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && !m_occupied)
        emit createRequested(m_slot);
    QWidget::mousePressEvent(event);
}

void CuePad::mouseDoubleClickEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && m_occupied)
        emit renameRequested(m_slot);
    QWidget::mouseDoubleClickEvent(event);
}

void CuePad::contextMenuEvent(QContextMenuEvent* event)
{
    if (!m_occupied) return;

    QMenu menu(this);
    QAction* renameAct = menu.addAction(QStringLiteral("Rename"));
    QAction* delAct    = menu.addAction(QStringLiteral("Delete"));
    QAction* chosen    = menu.exec(event->globalPos());

    if (chosen == renameAct)
        emit renameRequested(m_slot);
    else if (chosen == delAct)
        emit deleteRequested(m_slot);
}

void CuePad::enterEvent(QEnterEvent* event)
{
    m_hovered = true;
    update();
    QWidget::enterEvent(event);
}

void CuePad::leaveEvent(QEvent* event)
{
    m_hovered = false;
    update();
    QWidget::leaveEvent(event);
}

// ─────────────────────────────────────────────────────────────────────────────
// CuePointEditor
// ─────────────────────────────────────────────────────────────────────────────

CuePointEditor::CuePointEditor(Database* db, QWidget* parent)
    : QWidget(parent)
    , m_db(db)
{
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(Theme::Layout::GapSm);

    // Section label
    auto* label = new QLabel(QStringLiteral("HOT CUES"), this);
    label->setTextInteractionFlags(Qt::NoTextInteraction);
    {
        QFont f = label->font();
        f.setPointSize(Theme::Font::Small);
        f.setCapitalization(QFont::AllUppercase);
        f.setWeight(QFont::Medium);
        label->setFont(f);
        label->setStyleSheet(QString("color: %1;").arg(Theme::Color::Text3));
    }
    outer->addWidget(label, 0, Qt::AlignLeft);

    // Pad row
    auto* padRow = new QHBoxLayout();
    padRow->setContentsMargins(0, 0, 0, 0);
    padRow->setSpacing(kPadSpacing);

    m_pads.reserve(8);
    for (int i = 0; i < 8; ++i) {
        auto* pad = new CuePad(i, this);
        m_pads.append(pad);
        padRow->addWidget(pad);
        connect(pad, &CuePad::createRequested, this, &CuePointEditor::onCreateRequested);
        connect(pad, &CuePad::deleteRequested, this, &CuePointEditor::onDeleteRequested);
        connect(pad, &CuePad::renameRequested, this, &CuePointEditor::onRenameRequested);
    }
    padRow->addStretch();
    outer->addLayout(padRow);
}

void CuePointEditor::loadCues(long long songId)
{
    m_songId = songId;
    m_cues   = m_db->loadCuePoints(songId);
    refresh();
}

void CuePointEditor::clear()
{
    m_songId = -1;
    m_cues.clear();
    for (CuePad* pad : m_pads)
        pad->setCue(nullptr);
}

void CuePointEditor::refresh()
{
    // Index cues by slot for quick lookup
    QHash<int, const CuePoint*> bySlot;
    for (const CuePoint& c : m_cues) {
        if (c.cue_type == CueType::HotCue && c.slot >= 0 && c.slot < 8)
            bySlot[c.slot] = &c;
    }
    for (int i = 0; i < 8; ++i)
        m_pads[i]->setCue(bySlot.value(i, nullptr));
}

void CuePointEditor::onCreateRequested(int slot)
{
    if (m_songId <= 0) return;

    bool ok = false;
    const QString name = QInputDialog::getText(
        this, QStringLiteral("New Hot Cue"),
        QStringLiteral("Label (optional):"),
        QLineEdit::Normal, QString(), &ok);
    if (!ok) return;

    CuePoint cue;
    cue.song_id     = m_songId;
    cue.cue_type    = CueType::HotCue;
    cue.slot        = slot;
    cue.position_ms = 0;
    cue.end_ms      = -1;
    cue.name        = name.toStdString();
    cue.color       = (slot % 8) + 1;  // default: cycle through palette
    cue.sort_order  = slot;

    if (m_db->insertCuePoint(cue)) {
        m_cues.append(cue);
        refresh();
    }
}

void CuePointEditor::onDeleteRequested(int slot)
{
    for (int i = 0; i < m_cues.size(); ++i) {
        const CuePoint& c = m_cues[i];
        if (c.cue_type == CueType::HotCue && c.slot == slot) {
            m_db->deleteCuePoint(c.id);
            m_cues.removeAt(i);
            refresh();
            return;
        }
    }
}

void CuePointEditor::onRenameRequested(int slot)
{
    for (CuePoint& c : m_cues) {
        if (c.cue_type != CueType::HotCue || c.slot != slot) continue;

        bool ok = false;
        const QString newName = QInputDialog::getText(
            this, QStringLiteral("Rename Hot Cue"),
            QStringLiteral("Label:"),
            QLineEdit::Normal,
            QString::fromStdString(c.name), &ok);
        if (!ok) return;

        c.name = newName.toStdString();
        m_db->updateCuePoint(c);
        refresh();
        return;
    }
}
