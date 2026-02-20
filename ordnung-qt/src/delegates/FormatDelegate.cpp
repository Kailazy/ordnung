#include "FormatDelegate.h"
#include "views/TrackTableView.h"
#include "models/TrackModel.h"
#include "views/table/LibraryTableColumn.h"
#include "views/table/LibraryTableRow.h"
#include "views/table/LibraryTableRowPainter.h"
#include "style/Theme.h"

#include <QPainter>
#include <QPainterPath>
#include <QPolygonF>
#include <QTableView>
#include <QComboBox>

FormatDelegate::FormatDelegate(QObject* parent)
    : QStyledItemDelegate(parent)
{}

void FormatDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option,
                           const QModelIndex& index) const
{
    auto* view = qobject_cast<TrackTableView*>(parent());
    if (!view || !view->trackModel() || !view->proxy())
        return;

    const QModelIndex sourceIdx = view->proxy()->mapToSource(index);
    if (!sourceIdx.isValid())
        return;

    TrackModel* tm = view->trackModel();
    const int srcRow = sourceIdx.row();
    if (srcRow < 0 || srcRow >= tm->tracks().size())
        return;

    const Track& track = tm->tracks()[srcRow];
    const bool rowExpanded = tm->data(sourceIdx, TrackModel::ExpandedRole).toBool();

    LibraryTableRow row(&track, srcRow);
    LibraryTableRowPainter::paintCell(painter, index.column(), option.rect, row,
                                     option, rowExpanded, view);

    // ── Play overlay — draw ▶ over thumbnail when hovering that zone ─────────
    if (index.column() == LibraryTableColumn::columnIndex(LibraryTableColumn::Title)) {
        if (view->hoveredRow() == index.row() && view->hoveredThumb()) {
            const int sz  = Theme::Layout::TrackThumbSize;
            const int pad = Theme::Layout::TrackThumbPad;
            const QRect cell = option.rect;
            const int ty = cell.top() + (cell.height() - sz) / 2;
            const QRect thumbRect(cell.left() + pad, ty, sz, sz);

            painter->save();
            painter->setRenderHint(QPainter::Antialiasing);

            // Dark scrim over thumbnail
            QPainterPath scrim;
            scrim.addRoundedRect(QRectF(thumbRect), 2, 2);
            painter->fillPath(scrim, QColor(0, 0, 0, 160));

            // ▶ triangle, centred
            const QPointF c = QRectF(thumbRect).center();
            const qreal ts  = sz / 5.0;
            QPolygonF tri;
            tri << QPointF(c.x() - ts * 0.6, c.y() - ts)
                << QPointF(c.x() + ts,        c.y())
                << QPointF(c.x() - ts * 0.6,  c.y() + ts);
            painter->setBrush(QColor(Theme::Color::Accent));
            painter->setPen(Qt::NoPen);
            painter->drawPolygon(tri);

            painter->restore();
        }
    }
}

QSize FormatDelegate::sizeHint(const QStyleOptionViewItem& option,
                                const QModelIndex& index) const
{
    Q_UNUSED(option)
    Q_UNUSED(index)
    return QSize(20, Theme::Layout::TrackRowH);
}

QWidget* FormatDelegate::createEditor(QWidget* parent, const QStyleOptionViewItem& option,
                                      const QModelIndex& index) const
{
    Q_UNUSED(option)
    if (index.column() == LibraryTableColumn::columnIndex(LibraryTableColumn::Format)) {
        auto* cb = new QComboBox(parent);
        for (const char* fmt : {"mp3","flac","wav","aiff","alac","ogg","m4a","wma","aac"})
            cb->addItem(fmt);
        cb->setEditable(true);
        return cb;
    }
    return QStyledItemDelegate::createEditor(parent, option, index);
}

void FormatDelegate::setEditorData(QWidget* editor, const QModelIndex& index) const
{
    const QString value = index.data(Qt::EditRole).toString();
    if (auto* cb = qobject_cast<QComboBox*>(editor)) {
        const int i = cb->findText(value, Qt::MatchFixedString);
        if (i >= 0)
            cb->setCurrentIndex(i);
        else
            cb->setCurrentText(value);
    } else {
        QStyledItemDelegate::setEditorData(editor, index);
    }
}

void FormatDelegate::setModelData(QWidget* editor, QAbstractItemModel* model,
                                   const QModelIndex& index) const
{
    if (auto* cb = qobject_cast<QComboBox*>(editor)) {
        model->setData(index, cb->currentText().trimmed(), Qt::EditRole);
    } else {
        QStyledItemDelegate::setModelData(editor, model, index);
    }
}
