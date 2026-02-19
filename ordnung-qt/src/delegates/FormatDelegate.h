#pragma once

#include <QStyledItemDelegate>
#include <QColor>
#include <QMap>

// FormatDelegate paints format badges (FLAC, AIFF, MP3, etc.) and the
// title+artist two-line cell for column ColTitle.
// It also paints the expand chevron in column ColChevron.
//
// All painting is done via QPainter — no embedded widgets.
class FormatDelegate : public QStyledItemDelegate
{
    Q_OBJECT
public:
    explicit FormatDelegate(QObject* parent = nullptr);

    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override;

    QSize sizeHint(const QStyleOptionViewItem& option,
                   const QModelIndex& index) const override;

    // For title+artist column, we need the artist via a secondary role.
    // TrackModel provides it at Qt::UserRole + 4.
    static constexpr int ArtistRole = Qt::UserRole + 4;

private:
    struct BadgeColors {
        QColor text;
        QColor bg;
    };

    static const QMap<QString, BadgeColors>& colorMap();
    static BadgeColors colorsForFormat(const QString& format);
    static void fillBackground(QPainter* p, const QStyleOptionViewItem& opt);
};
