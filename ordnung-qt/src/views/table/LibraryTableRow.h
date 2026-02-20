#pragma once

#include "core/Track.h"

// Row as data wrapper: one row = one track. The library table is row-centric;
// each row is the single unit of data (a track) with multiple column attributes.
struct LibraryTableRow {
    const Track* track = nullptr;
    int          rowIndex = -1;

    LibraryTableRow() = default;
    LibraryTableRow(const Track* t, int row) : track(t), rowIndex(row) {}

    bool isValid() const { return track != nullptr && rowIndex >= 0; }
};
