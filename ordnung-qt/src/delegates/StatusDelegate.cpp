#include "StatusDelegate.h"
#include "models/DownloadsModel.h"

#include <QPainter>
#include <QPainterPath>
#include <QApplication>
#include <QFontMetrics>
#include <QStyle>
#include <QMap>

// ── Color helpers ──────────────────────────────────────────────────────────

StatusDelegate::BadgeColors StatusDelegate::colorsForExt(const QString& ext)
{
    // Same mapping as FormatDelegate — reuse design token colors.
    static const QMap<QString, BadgeColors> map = {
        { QStringLiteral("mp3"),  { QColor(0x44, 0x44, 0x44), QColor(0x1c, 0x1c, 0x1c) } },
        { QStringLiteral("flac"), { QColor(0x4f, 0xc3, 0xf7), QColor(0x0d, 0x3b, 0x4f) } },
        { QStringLiteral("wav"),  { QColor(0xce, 0x93, 0xd8), QColor(0x2a, 0x1a, 0x33) } },
        { QStringLiteral("aiff"), { QColor(0x81, 0xc7, 0x84), QColor(0x1b, 0x3a, 0x1b) } },
        { QStringLiteral("aif"),  { QColor(0x81, 0xc7, 0x84), QColor(0x1b, 0x3a, 0x1b) } },
        { QStringLiteral("ogg"),  { QColor(0xff, 0xb7, 0x4d), QColor(0x3b, 0x2a, 0x0d) } },
        { QStringLiteral("m4a"),  { QColor(0xf4, 0x8f, 0xb1), QColor(0x3b, 0x15, 0x25) } },
        { QStringLiteral("alac"), { QColor(0x4d, 0xb6, 0xac), QColor(0x0d, 0x3b, 0x35) } },
        { QStringLiteral("wma"),  { QColor(0xe5, 0x73, 0x73), QColor(0x3b, 0x15, 0x15) } },
        { QStringLiteral("aac"),  { QColor(0xae, 0xd5, 0x81), QColor(0x1f, 0x3a, 0x0d) } },
    };
    auto it = map.find(ext.toLower());
    if (it != map.end()) return it.value();
    return { QColor(0x44, 0x44, 0x44), QColor(0x1c, 0x1c, 0x1c) };
}

StatusDelegate::BadgeColors StatusDelegate::colorsForStatus(const QString& status)
{
    if (status == QLatin1String("pending"))
        return { QColor(0xff, 0xb7, 0x4d), QColor(0x3b, 0x2a, 0x0d) };
    if (status == QLatin1String("converting"))
        return { QColor(0x4f, 0xc3, 0xf7), QColor(0x0d, 0x3b, 0x4f) };
    if (status == QLatin1String("done"))
        return { QColor(0x81, 0xc7, 0x84), QColor(0x1b, 0x3a, 0x1b) };
    if (status == QLatin1String("failed"))
        return { QColor(0xe5, 0x73, 0x73), QColor(0x3b, 0x15, 0x15) };
    return { QColor(0x44, 0x44, 0x44), QColor(0x1c, 0x1c, 0x1c) };
}

void StatusDelegate::fillBackground(QPainter* p, const QStyleOptionViewItem& opt)
{
    const bool selected = opt.state & QStyle::State_Selected;
    const bool hovered  = opt.state & QStyle::State_MouseOver;

    if (selected && hovered)     p->fillRect(opt.rect, QColor(0x0f, 0x40, 0x60));
    else if (selected)           p->fillRect(opt.rect, QColor(0x0d, 0x3b, 0x4f));
    else if (hovered)            p->fillRect(opt.rect, QColor(0x0d, 0x0d, 0x0d));
    else                         p->fillRect(opt.rect, QColor(0x08, 0x08, 0x08));
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
        f.setPointSize(15);
        painter->setFont(f);
        painter->setPen(QColor(0xd0, 0xd0, 0xd0));
        const QRect r = option.rect.adjusted(12, 0, -8, 0);
        const QFontMetrics fm(f);
        painter->drawText(r, Qt::AlignVCenter | Qt::AlignLeft,
                          fm.elidedText(text, Qt::ElideRight, r.width()));

    } else if (col == DownloadsModel::ColExt) {
        // Extension — colored mono text, no background pill (per spec Section 5)
        const QString ext  = index.data(Qt::DisplayRole).toString();
        const BadgeColors c = colorsForExt(ext.toLower());

        QFont f(QStringLiteral("Consolas"));
        f.setStyleHint(QFont::Monospace);
        f.setPointSize(13);
        painter->setFont(f);
        painter->setPen(c.text);
        painter->drawText(option.rect, Qt::AlignCenter, ext);

    } else if (col == DownloadsModel::ColSize) {
        const QString text = index.data(Qt::DisplayRole).toString();
        QFont f(QStringLiteral("Consolas"));
        f.setStyleHint(QFont::Monospace);
        f.setPointSize(13);
        painter->setFont(f);
        painter->setPen(QColor(0x77, 0x77, 0x77));
        painter->drawText(option.rect, Qt::AlignCenter, text);

    } else if (col == DownloadsModel::ColStatus) {
        // Status badge — rounded rect pill
        const QString status = index.data(Qt::DisplayRole).toString();
        if (status.isEmpty()) {
            painter->restore();
            return;
        }

        const BadgeColors c = colorsForStatus(status);
        const QString label = status.toUpper();

        QFont badgeFont(QStringLiteral("Consolas"));
        badgeFont.setStyleHint(QFont::Monospace);
        badgeFont.setPointSize(11);
        badgeFont.setWeight(QFont::DemiBold);
        painter->setFont(badgeFont);

        const QFontMetrics fm(badgeFont);
        const int textW = fm.horizontalAdvance(label);

        const int badgeW = textW + 18;
        const int badgeH = 20;
        const int badgeX = option.rect.left() + (option.rect.width()  - badgeW) / 2;
        const int badgeY = option.rect.top()  + (option.rect.height() - badgeH) / 2;
        const QRect badgeRect(badgeX, badgeY, badgeW, badgeH);

        QPainterPath path;
        path.addRoundedRect(badgeRect, 3, 3);
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
        const QColor textColor = rowHovered ? QColor(0x4f, 0xc3, 0xf7) : QColor(0x44, 0x44, 0x44);

        QFont f = QApplication::font();
        f.setPointSize(13);
        painter->setFont(f);
        painter->setPen(textColor);
        painter->drawText(option.rect, Qt::AlignCenter, text);
    }

    painter->restore();
}

QSize StatusDelegate::sizeHint(const QStyleOptionViewItem& /*option*/,
                                const QModelIndex& /*index*/) const
{
    return QSize(80, 46);
}
