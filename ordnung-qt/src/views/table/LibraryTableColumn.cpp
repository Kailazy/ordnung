#include "LibraryTableColumn.h"

namespace LibraryTableColumn {

static const struct {
    bool critical;
    int  fixedWidth;
    const char* header;
} kColumns[] = {
    { true,  320, "TRACK"   },  // 0 Title
    { true,  180, "ARTIST"  },  // 1 Artist
    { false, 72,  "BPM"     },  // 2
    { false, 64,  "KEY"     },  // 3
    { false, 72,  "TIME"    },  // 4
    { false, 80,  "FORMAT"  },  // 5
    { false, 24,  ""        },  // 6 Color (dot, no header text)
    { false, 20,  ""        },  // 7 Prepared (green dot when prepared, no header text)
    { false, 64,  "KBPS"    },  // 8 Bitrate
    { false, 160, "COMMENT" },  // 9 Comment
};

int columnCount()
{
    return static_cast<int>(sizeof(kColumns) / sizeof(kColumns[0]));
}

ColumnProps columnProps(int columnIndex)
{
    if (columnIndex < 0 || columnIndex >= columnCount())
        return { false, 80 };
    return { kColumns[columnIndex].critical, kColumns[columnIndex].fixedWidth };
}

QString headerText(int columnIndex)
{
    if (columnIndex < 0 || columnIndex >= columnCount())
        return QString();
    return QString::fromUtf8(kColumns[columnIndex].header);
}

ColumnRole columnRole(int columnIndex)
{
    if (columnIndex < 0 || columnIndex >= columnCount())
        return Title; // fallback
    return static_cast<ColumnRole>(columnIndex);
}

int columnIndex(ColumnRole role)
{
    const int i = static_cast<int>(role);
    return (i >= 0 && i < columnCount()) ? i : 0;
}

} // namespace LibraryTableColumn
