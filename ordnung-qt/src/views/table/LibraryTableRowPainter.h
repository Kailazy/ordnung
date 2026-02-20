#pragma once

#include "LibraryTableRow.h"

#include <QStyleOptionViewItem>
#include <QModelIndex>

class QPainter;
class QTableView;

// Paints one cell of a library table row. Row-centric: each row is one track;
// the painter draws the cell content and, for the first column, the row
// divider line so rows are clearly separated.
namespace LibraryTableRowPainter {

void paintCell(QPainter* painter,
               int columnIndex,
               const QRect& cellRect,
               const LibraryTableRow& row,
               const QStyleOptionViewItem& option,
               bool rowExpanded,
               QTableView* view);

} // namespace LibraryTableRowPainter
