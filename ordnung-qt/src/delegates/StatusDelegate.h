#pragma once

#include <QStyledItemDelegate>
#include <QPoint>

class DownloadsModel;

// StatusDelegate handles all columns of the downloads QTableView.
// Columns:
//   ColFilename  — plain text, left-aligned
//   ColExt       — monospace colored text (uses format color map, no bg pill)
//   ColSize      — plain mono text
//   ColStatus    — rounded-rect status badge (pending/converting/done/failed)
//   ColAction    — text button "convert" or "retry" (color changes on row hover)
//
// Click detection for ColAction is done via the view's mousePressEvent feeding
// into the signal convertRequested(QModelIndex).
class StatusDelegate : public QStyledItemDelegate
{
    Q_OBJECT
public:
    explicit StatusDelegate(QObject* parent = nullptr);

    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override;

    QSize sizeHint(const QStyleOptionViewItem& option,
                   const QModelIndex& index) const override;

    // Playlist panel delegate reuse:
    // hover row index tracked externally via setHoveredRow().
    void setHoveredRow(int row) { m_hoveredRow = row; }
    int  hoveredRow() const     { return m_hoveredRow; }

signals:
    void convertRequested(const QModelIndex& index);

private:
    struct BadgeColors { QColor text; QColor bg; };

    static BadgeColors colorsForExt(const QString& ext);
    static BadgeColors colorsForStatus(const QString& status);
    static void        fillBackground(QPainter* p, const QStyleOptionViewItem& opt);

    int m_hoveredRow = -1;
};
