#pragma once

#include <QString>

// Column registry for the library table. Single source of truth for column
// count, roles, properties, and headers.
//
// To add a column:
// 1. Add an enum value to ColumnRole (and extend kColumns in .cpp with header + props).
// 2. Handle the new role in TrackModel::data/setData (and headerData if alignment differs).
// 3. Handle the new role in LibraryTableRowPainter::paintCell (and in FormatDelegate if it needs a custom editor).
namespace LibraryTableColumn {

enum ColumnRole {
    Title    = 0,
    Artist   = 1,
    Bpm      = 2,
    Key      = 3,
    Time     = 4,
    Format   = 5,
    Color    = 6,   // Pioneer color label dot (24px, click-to-cycle)
    Prepared = 7,   // Preparation mode indicator dot (20px, green when prepared)
    Bitrate  = 8,   // Audio bitrate in kbps
    Comment  = 9,   // User comment / annotation
    // Add new roles here; keep in sync with kColumns in LibraryTableColumn.cpp.
};

struct ColumnProps {
    bool    critical;   // If true, shown in minimal view when window shrinks
    int     fixedWidth; // Width in pixels when visible
};

int          columnCount();
ColumnProps  columnProps(int columnIndex);
QString      headerText(int columnIndex);

// Role ↔ index. Use these instead of raw indices so reordering/adding columns
// only touches this module.
ColumnRole   columnRole(int columnIndex);
int          columnIndex(ColumnRole role);

} // namespace LibraryTableColumn
