#include "FormatDelegate.h"
#include "models/TrackModel.h"

#include <QPainter>
#include <QPainterPath>
#include <QApplication>
#include <QFontMetrics>
#include <QStyle>

// ── Static color map ───────────────────────────────────────────────────────

const QMap<QString, FormatDelegate::BadgeColors>& FormatDelegate::colorMap()
{
    static const QMap<QString, BadgeColors> map = {
        { QStringLiteral("mp3"),  { QColor(0x44, 0x44, 0x44), QColor(0x1c, 0x1c, 0x1c) } },
        { QStringLiteral("flac"), { QColor(0x4f, 0xc3, 0xf7), QColor(0x0d, 0x3b, 0x4f) } },
        { QStringLiteral("wav"),  { QColor(0xce, 0x93, 0xd8), QColor(0x2a, 0x1a, 0x33) } },
        { QStringLiteral("aiff"), { QColor(0x81, 0xc7, 0x84), QColor(0x1b, 0x3a, 0x1b) } },
        { QStringLiteral("ogg"),  { QColor(0xff, 0xb7, 0x4d), QColor(0x3b, 0x2a, 0x0d) } },
        { QStringLiteral("m4a"),  { QColor(0xf4, 0x8f, 0xb1), QColor(0x3b, 0x15, 0x25) } },
        { QStringLiteral("alac"), { QColor(0x4d, 0xb6, 0xac), QColor(0x0d, 0x3b, 0x35) } },
        { QStringLiteral("wma"),  { QColor(0xe5, 0x73, 0x73), QColor(0x3b, 0x15, 0x15) } },
        { QStringLiteral("aac"),  { QColor(0xae, 0xd5, 0x81), QColor(0x1f, 0x3a, 0x0d) } },
    };
    return map;
}

FormatDelegate::BadgeColors FormatDelegate::colorsForFormat(const QString& format)
{
    const auto& map = colorMap();
    auto it = map.find(format.toLower());
    if (it != map.end())
        return it.value();
    return { QColor(0x44, 0x44, 0x44), QColor(0x1c, 0x1c, 0x1c) };
}

// ── Row background fill ────────────────────────────────────────────────────

void FormatDelegate::fillBackground(QPainter* p, const QStyleOptionViewItem& opt)
{
    const bool selected = opt.state & QStyle::State_Selected;
    const bool hovered  = opt.state & QStyle::State_MouseOver;

    if (selected && hovered) {
        p->fillRect(opt.rect, QColor(0x0f, 0x40, 0x60));
    } else if (selected) {
        p->fillRect(opt.rect, QColor(0x0d, 0x3b, 0x4f));
    } else if (hovered) {
        p->fillRect(opt.rect, QColor(0x0d, 0x0d, 0x0d));
    } else {
        p->fillRect(opt.rect, QColor(0x08, 0x08, 0x08));
    }
}

// ── Constructor ────────────────────────────────────────────────────────────

FormatDelegate::FormatDelegate(QObject* parent)
    : QStyledItemDelegate(parent)
{}

// ── paint() — the spec requires exact step-by-step geometry ───────────────

void FormatDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option,
                           const QModelIndex& index) const
{
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setRenderHint(QPainter::TextAntialiasing, true);

    const int col = index.column();

    // ── Fill row background ──────────────────────────────────────────────
    fillBackground(painter, option);

    // ── Column-specific painting ─────────────────────────────────────────
    if (col == TrackModel::ColChevron) {
        // Chevron: ▸ (collapsed) or ▾ (expanded)
        const bool expanded = index.data(TrackModel::ExpandedRole).toBool();
        const QString ch = expanded ? QStringLiteral("▾") : QStringLiteral("▸");
        const QColor color = expanded ? QColor(0x4f, 0xc3, 0xf7) : QColor(0x44, 0x44, 0x44);

        QFont f = QApplication::font();
        f.setPointSize(12);
        painter->setFont(f);
        painter->setPen(color);
        painter->drawText(option.rect, Qt::AlignCenter, ch);

    } else if (col == TrackModel::ColTitle) {
        // Two-line cell: title (17pt #e8e8e8) + artist (15pt #777777)
        const QString title  = index.data(Qt::DisplayRole).toString();
        const QString artist = index.data(ArtistRole).toString();

        const QRect r = option.rect.adjusted(12, 0, -8, 0);

        // Title font
        QFont titleFont = QApplication::font();
        titleFont.setPointSize(17);
        titleFont.setWeight(QFont::Normal);
        QFontMetrics titleFm(titleFont);
        const QString titleElided = titleFm.elidedText(title, Qt::ElideRight, r.width());

        // Artist font
        QFont artistFont = QApplication::font();
        artistFont.setPointSize(15);
        artistFont.setWeight(QFont::Normal);
        QFontMetrics artistFm(artistFont);
        const QString artistElided = artist.isEmpty()
            ? QString()
            : artistFm.elidedText(artist, Qt::ElideRight, r.width());

        const int totalH    = titleFm.height() + (artist.isEmpty() ? 0 : 5 + artistFm.height());
        const int startY    = r.top() + (r.height() - totalH) / 2;

        // Draw title
        painter->setFont(titleFont);
        painter->setPen(QColor(0xe8, 0xe8, 0xe8));
        painter->drawText(QRect(r.left(), startY, r.width(), titleFm.height()),
                          Qt::AlignLeft | Qt::AlignTop, titleElided);

        // Draw artist
        if (!artist.isEmpty()) {
            const int artistY = startY + titleFm.height() + 5;
            painter->setFont(artistFont);
            painter->setPen(QColor(0x77, 0x77, 0x77));
            painter->drawText(QRect(r.left(), artistY, r.width(), artistFm.height()),
                              Qt::AlignLeft | Qt::AlignTop, artistElided);
        }

    } else if (col == TrackModel::ColBpm || col == TrackModel::ColKey
               || col == TrackModel::ColTime) {
        // Mono data columns
        const QString text = index.data(Qt::DisplayRole).toString();
        if (text.isEmpty()) {
            painter->restore();
            return;
        }
        QFont f;
        f.setFamily(QStringLiteral("Consolas"));
        QFont fallback(QStringLiteral("Courier New"));
        f.setStyleHint(QFont::Monospace);
        f.setPointSize(13);
        painter->setFont(f);
        painter->setPen(QColor(0x77, 0x77, 0x77));
        painter->drawText(option.rect, Qt::AlignCenter, text);

    } else if (col == TrackModel::ColFormat) {
        // Format badge — exact spec from DESIGN_SPEC.md Section 3
        const QString format = index.data(Qt::DisplayRole).toString().toLower();
        if (format.isEmpty()) {
            painter->restore();
            return;
        }

        const BadgeColors colors = colorsForFormat(format);
        const QString formatUpper = format.toUpper();

        // Step 3: Font
        QFont badgeFont(QStringLiteral("Consolas"));
        badgeFont.setStyleHint(QFont::Monospace);
        badgeFont.setPointSize(11);
        badgeFont.setWeight(QFont::DemiBold);
        painter->setFont(badgeFont);

        // Step 4: Measure text width
        const QFontMetrics fm(badgeFont);
        const int textWidth = fm.horizontalAdvance(formatUpper);

        // Step 5: Badge rect geometry
        const int badgeW   = textWidth + 14;  // 7px each side
        const int badgeH   = 20;
        const int badgeX   = option.rect.left() + (option.rect.width() - badgeW) / 2;
        const int badgeY   = option.rect.top()  + (option.rect.height() - badgeH) / 2;
        const QRect badgeRect(badgeX, badgeY, badgeW, badgeH);

        // Step 8: Fill badge background
        QPainterPath path;
        path.addRoundedRect(badgeRect, 3, 3);
        painter->fillPath(path, colors.bg);

        // Step 9: Draw text uppercase centered in badge
        painter->setPen(colors.text);
        painter->drawText(badgeRect, Qt::AlignCenter, formatUpper);
    }

    painter->restore();
}

QSize FormatDelegate::sizeHint(const QStyleOptionViewItem& option,
                                const QModelIndex& index) const
{
    Q_UNUSED(option)
    Q_UNUSED(index)
    // Row height is controlled by the view's verticalHeader().
    // Return a minimal hint so the view's setDefaultSectionSize(54) governs.
    return QSize(20, 54);
}
