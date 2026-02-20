#include "LibraryTableRowPainter.h"
#include "LibraryTableColumn.h"

#include "style/Theme.h"

#include <QPainter>
#include <QPainterPath>
#include <QApplication>
#include <QFontMetrics>
#include <QStyle>
#include <QTableView>
#include <QDateTime>

namespace LibraryTableRowPainter {

static void fillBackground(QPainter* p, const QStyleOptionViewItem& opt, bool expanded)
{
    const bool selected = opt.state & QStyle::State_Selected;
    const bool hovered  = opt.state & QStyle::State_MouseOver;

    if (selected && hovered)
        p->fillRect(opt.rect, QColor(Theme::Color::RowSelHov));
    else if (selected)
        p->fillRect(opt.rect, QColor(Theme::Color::AccentBg));
    else if (hovered)
        p->fillRect(opt.rect, QColor(Theme::Color::RowHov));
    else if (expanded)
        p->fillRect(opt.rect, QColor(Theme::Color::RowExpanded));
    else
        p->fillRect(opt.rect, QColor(Theme::Color::Bg));
}

void paintCell(QPainter* painter,
               int columnIndex,
               const QRect& cellRect,
               const LibraryTableRow& row,
               const QStyleOptionViewItem& option,
               bool rowExpanded,
               QTableView* view)
{
    if (!row.isValid() || row.track == nullptr)
        return;

    const Track& t = *row.track;
    const int colCount = LibraryTableColumn::columnCount();

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setRenderHint(QPainter::TextAntialiasing, true);

    fillBackground(painter, option, rowExpanded);

    const LibraryTableColumn::ColumnRole role = LibraryTableColumn::columnRole(columnIndex);
    const bool isFirstColumn = (columnIndex == LibraryTableColumn::columnIndex(LibraryTableColumn::Title));

    if (isFirstColumn && rowExpanded) {
        painter->fillRect(QRect(cellRect.left(), cellRect.top(), 2, cellRect.height()),
                         QColor(Theme::Color::Accent));
    }

    const QString title   = QString::fromStdString(t.title);
    const QString artist  = QString::fromStdString(t.artist);
    const double  bpm     = t.bpm;
    const QString keySig  = QString::fromStdString(t.key_sig);
    const QString timeStr = QString::fromStdString(t.time);
    const QString format  = QString::fromStdString(t.format.empty() ? "mp3" : t.format);

    switch (role) {
    case LibraryTableColumn::Title: {
        const int thumbSize = Theme::Layout::TrackThumbSize;
        const int thumbPad  = Theme::Layout::TrackThumbPad;
        const int thumbX    = cellRect.left() + thumbPad;
        const int thumbY    = cellRect.top() + (cellRect.height() - thumbSize) / 2;
        const QRect thumbRect(thumbX, thumbY, thumbSize, thumbSize);

        QPainterPath path;
        path.addRoundedRect(thumbRect, 2, 2);
        painter->fillPath(path, QColor(Theme::Color::Bg3));
        painter->setPen(QColor(Theme::Color::Text3));
        painter->drawPath(path);

        // Analyzing indicator — spinning arc overlay when background ffprobe is running
        if (t.is_analyzing) {
            const qint64 frame = QDateTime::currentMSecsSinceEpoch() / 120;
            const int startAngle = static_cast<int>((frame % 30) * 12) * 16;  // rotates every 3.6s
            QPen arcPen{QColor(Theme::Color::Accent)};
            arcPen.setWidth(2);
            arcPen.setCapStyle(Qt::RoundCap);
            painter->setPen(arcPen);
            painter->setBrush(Qt::NoBrush);
            painter->drawArc(thumbRect.adjusted(4, 4, -4, -4), startAngle, 120 * 16);
        }

        const int titleLeft = thumbX + thumbSize + Theme::Layout::GapSm;
        const QRect titleRect(titleLeft, cellRect.top(),
                              cellRect.right() - titleLeft - Theme::Layout::TrackCellPadH,
                              cellRect.height());
        QFont f = QApplication::font();
        f.setPointSize(Theme::Font::Body);
        f.setWeight(QFont::Normal);
        QFontMetrics fm(f);
        painter->setFont(f);
        painter->setPen(QColor(Theme::Color::TextBright));
        painter->drawText(titleRect, Qt::AlignLeft | Qt::AlignVCenter,
                          fm.elidedText(title, Qt::ElideRight, titleRect.width()));
        break;
    }
    case LibraryTableColumn::Artist: {
        const QRect r = cellRect.adjusted(Theme::Layout::TrackCellPadH, 0,
                                          -Theme::Layout::TrackCellPadH, 0);
        QFont f = QApplication::font();
        f.setPointSize(Theme::Font::Caption);
        f.setWeight(QFont::Normal);
        QFontMetrics fm(f);
        painter->setFont(f);
        painter->setPen(QColor(Theme::Color::Text2));
        painter->drawText(r, Qt::AlignLeft | Qt::AlignVCenter,
                          fm.elidedText(artist, Qt::ElideRight, r.width()));
        break;
    }
    case LibraryTableColumn::Bpm:
    case LibraryTableColumn::Key:
    case LibraryTableColumn::Time: {
        QString text;
        if (role == LibraryTableColumn::Bpm)
            text = bpm > 0 ? QString::number(static_cast<int>(bpm)) : QString();
        else if (role == LibraryTableColumn::Key)
            text = keySig;
        else
            text = timeStr;
        const QString display = text.isEmpty() ? QStringLiteral("--") : text;
        const bool rightAlign = (role == LibraryTableColumn::Bpm || role == LibraryTableColumn::Time);
        const QRect r = cellRect.adjusted(Theme::Layout::TrackCellPadH, 0,
                                          -Theme::Layout::TrackCellPadH, 0);
        QFont f{QLatin1String(Theme::Font::Mono)};
        f.setStyleHint(QFont::Monospace);
        f.setPointSize(Theme::Font::Meta);
        painter->setFont(f);
        painter->setPen(QColor(text.isEmpty() ? Theme::Color::Text3 : Theme::Color::Text2));
        painter->drawText(r, (rightAlign ? Qt::AlignRight : Qt::AlignCenter) | Qt::AlignVCenter, display);
        break;
    }
    case LibraryTableColumn::Format:
        if (format.isEmpty()) {
            const QRect r = cellRect.adjusted(Theme::Layout::TrackCellPadH, 0,
                                             -Theme::Layout::TrackCellPadH, 0);
            QFont f{QLatin1String(Theme::Font::Mono)};
            f.setPointSize(Theme::Font::Meta);
            painter->setFont(f);
            painter->setPen(QColor(Theme::Color::Text3));
            painter->drawText(r, Qt::AlignCenter, QStringLiteral("--"));
        } else {
            const Theme::Badge::Colors colors = Theme::Badge::forFormat(format);
            const QString formatUpper = format.toUpper();
            QFont badgeFont{QLatin1String(Theme::Font::Mono)};
            badgeFont.setStyleHint(QFont::Monospace);
            badgeFont.setPointSize(Theme::Font::Badge);
            badgeFont.setWeight(QFont::DemiBold);
            painter->setFont(badgeFont);
            const QFontMetrics fm(badgeFont);
            const int textWidth = fm.horizontalAdvance(formatUpper);
            const int badgeW = textWidth + 2 * Theme::Badge::HPad;
            const int badgeH = Theme::Badge::Height;
            const int badgeX = cellRect.left() + (cellRect.width() - badgeW) / 2;
            const int badgeY = cellRect.top() + (cellRect.height() - badgeH) / 2;
            const QRect badgeRect(badgeX, badgeY, badgeW, badgeH);
            QPainterPath path;
            path.addRoundedRect(badgeRect, Theme::Badge::Radius, Theme::Badge::Radius);
            painter->fillPath(path, colors.bg);
            painter->setPen(colors.text);
            painter->drawText(badgeRect, Qt::AlignCenter, formatUpper);
        }
        break;
    case LibraryTableColumn::Color: {
        // Paint a filled circle in the Pioneer label color, centered in the narrow column.
        const QColor c = Theme::Color::labelColor(t.color_label);
        if (c.alpha() > 0) {
            const int dotR = 5;
            const QPoint center = cellRect.center();
            painter->setPen(Qt::NoPen);
            painter->setBrush(c);
            painter->drawEllipse(center, dotR, dotR);
        }
        break;
    }
    case LibraryTableColumn::Bitrate: {
        const QString text = t.bitrate > 0
            ? QString::number(t.bitrate)
            : QStringLiteral("--");
        const QRect r = cellRect.adjusted(Theme::Layout::TrackCellPadH, 0,
                                          -Theme::Layout::TrackCellPadH, 0);
        QFont f{QLatin1String(Theme::Font::Mono)};
        f.setStyleHint(QFont::Monospace);
        f.setPointSize(Theme::Font::Meta);
        painter->setFont(f);
        painter->setPen(QColor(t.bitrate > 0 ? Theme::Color::Text2 : Theme::Color::Text3));
        painter->drawText(r, Qt::AlignRight | Qt::AlignVCenter, text);
        break;
    }
    case LibraryTableColumn::Comment: {
        const QString text = QString::fromStdString(t.comment);
        const QRect r = cellRect.adjusted(Theme::Layout::TrackCellPadH, 0,
                                          -Theme::Layout::TrackCellPadH, 0);
        QFont f = QApplication::font();
        f.setPointSize(Theme::Font::Caption);
        QFontMetrics fm(f);
        painter->setFont(f);
        painter->setPen(QColor(text.isEmpty() ? Theme::Color::Text3 : Theme::Color::Text2));
        painter->drawText(r, Qt::AlignLeft | Qt::AlignVCenter,
                          fm.elidedText(text.isEmpty() ? QStringLiteral("—") : text,
                                        Qt::ElideRight, r.width()));
        break;
    }
    }

    // Vertical separator (right edge of cell, except last column)
    if (columnIndex < colCount - 1) {
        painter->fillRect(QRect(cellRect.right(), cellRect.top(), 1, cellRect.height()),
                         QColor(Theme::Color::RowSeparator));
    }

    // Row divider — full viewport width at bottom of row (drawn in every column so it appears even when scrolled)
    if (view && view->viewport()) {
        const int fullWidth = view->viewport()->width();
        painter->fillRect(QRect(0, cellRect.bottom(), fullWidth, 1),
                         QColor(Theme::Color::RowSeparator));
    }

    painter->restore();
}

} // namespace LibraryTableRowPainter
