#include "StatusDelegate.h"
#include "models/DownloadsModel.h"

#include <QPainter>
#include <QPainterPath>
#include <QApplication>
#include <QFontMetrics>
#include <QStyle>

// ── Row background fill ────────────────────────────────────────────────────

void StatusDelegate::fillBackground(QPainter* p, const QStyleOptionViewItem& opt)
{
    const bool selected = opt.state & QStyle::State_Selected;
    const bool hovered  = opt.state & QStyle::State_MouseOver;

    if (selected && hovered)     p->fillRect(opt.rect, QColor(Theme::Color::RowSelHov));
    else if (selected)           p->fillRect(opt.rect, QColor(Theme::Color::AccentBg));
    else if (hovered)            p->fillRect(opt.rect, QColor(Theme::Color::RowHov));
    else                         p->fillRect(opt.rect, QColor(Theme::Color::Bg));
}

// ── Constructor ────────────────────────────────────────────────────────────

StatusDelegate::StatusDelegate(QObject* parent)
    : QStyledItemDelegate(parent)
{}

// ── paint() ───────────────────────────────────────────────────────────────

void StatusDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option,
                           const QModelIndex& index) const
{
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setRenderHint(QPainter::TextAntialiasing, true);

    fillBackground(painter, option);

    const int col = index.column();

    if (col == DownloadsModel::ColFilename) {
        // Plain filename text, left-aligned, slightly inset
        const QString text = index.data(Qt::DisplayRole).toString();
        QFont f = QApplication::font();
        f.setPointSize(Theme::Font::Secondary);
        painter->setFont(f);
        painter->setPen(QColor(Theme::Color::Text));
        const QRect r = option.rect.adjusted(12, 0, -8, 0);
        const QFontMetrics fm(f);
        painter->drawText(r, Qt::AlignVCenter | Qt::AlignLeft,
                          fm.elidedText(text, Qt::ElideRight, r.width()));

    } else if (col == DownloadsModel::ColExt) {
        // Extension — colored mono text, no background pill (per spec Section 5)
        const QString ext = index.data(Qt::DisplayRole).toString();
        const QColor textColor = Theme::Badge::forFormat(ext.toLower()).text;

        QFont f(QLatin1String(Theme::Font::Mono));
        f.setStyleHint(QFont::Monospace);
        f.setPointSize(Theme::Font::Meta);
        painter->setFont(f);
        painter->setPen(textColor);
        painter->drawText(option.rect, Qt::AlignCenter, ext);

    } else if (col == DownloadsModel::ColSize) {
        const QString text = index.data(Qt::DisplayRole).toString();
        QFont f(QLatin1String(Theme::Font::Mono));
        f.setStyleHint(QFont::Monospace);
        f.setPointSize(Theme::Font::Meta);
        painter->setFont(f);
        painter->setPen(QColor(Theme::Color::Text2));
        painter->drawText(option.rect, Qt::AlignCenter, text);

    } else if (col == DownloadsModel::ColStatus) {
        // Status badge — rounded rect pill
        const QString status = index.data(Qt::DisplayRole).toString();
        if (status.isEmpty()) {
            painter->restore();
            return;
        }

        const Theme::Badge::Colors c = Theme::Badge::forStatus(status);
        const QString label = status.toUpper();

        QFont badgeFont(QLatin1String(Theme::Font::Mono));
        badgeFont.setStyleHint(QFont::Monospace);
        badgeFont.setPointSize(Theme::Font::Badge);
        badgeFont.setWeight(QFont::DemiBold);
        painter->setFont(badgeFont);

        const QFontMetrics fm(badgeFont);
        const int textW = fm.horizontalAdvance(label);

        const int badgeW = textW + 2 * Theme::Badge::HPad;
        const int badgeH = Theme::Badge::Height;
        const int badgeX = option.rect.left() + (option.rect.width()  - badgeW) / 2;
        const int badgeY = option.rect.top()  + (option.rect.height() - badgeH) / 2;
        const QRect badgeRect(badgeX, badgeY, badgeW, badgeH);

        QPainterPath path;
        path.addRoundedRect(badgeRect, Theme::Badge::Radius, Theme::Badge::Radius);
        painter->fillPath(path, c.bg);

        painter->setPen(c.text);
        painter->drawText(badgeRect, Qt::AlignCenter, label);

    } else if (col == DownloadsModel::ColAction) {
        // Text button "convert" or "retry"
        const QString text = index.data(Qt::DisplayRole).toString();
        if (text.isEmpty()) {
            painter->restore();
            return;
        }

        const bool rowHovered = (index.row() == m_hoveredRow);
        const QColor textColor = rowHovered ? QColor(Theme::Color::Accent)
                                            : QColor(Theme::Color::Text3);

        QFont f = QApplication::font();
        f.setPointSize(Theme::Font::Meta);
        painter->setFont(f);
        painter->setPen(textColor);
        painter->drawText(option.rect, Qt::AlignCenter, text);
    }

    painter->restore();
}

QSize StatusDelegate::sizeHint(const QStyleOptionViewItem& /*option*/,
                                const QModelIndex& /*index*/) const
{
    return QSize(80, Theme::Layout::DownloadRowH);
}
