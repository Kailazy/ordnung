#include "PdbWriter.h"

#include "core/Track.h"
#include "core/Playlist.h"
#include "core/CuePoint.h"

#include <QFile>
#include <QDataStream>
#include <QByteArray>
#include <QDebug>
#include <QSet>
#include <QtEndian>

#include <algorithm>
#include <cstring>
#include <vector>

// ─────────────────────────────────────────────────────────────────────────────
// Constants from rekordbox_pdb.ksy
// ─────────────────────────────────────────────────────────────────────────────

static constexpr quint32 PAGE_SIZE        = 4096;   // len_page
static constexpr quint32 PAGE_HEAP_OFFSET = 0x28;   // heap data starts here
static constexpr quint32 ROW_GROUP_SIZE   = 0x24;   // 36 bytes per row group (2+2+32)
static constexpr int     ROWS_PER_GROUP   = 16;

// page_type enum values (rekordbox_pdb.ksy: page_type)
static constexpr quint32 TABLE_TRACKS          = 0;
static constexpr quint32 TABLE_GENRES          = 1;
static constexpr quint32 TABLE_ARTISTS         = 2;
static constexpr quint32 TABLE_ALBUMS          = 3;
static constexpr quint32 TABLE_LABELS          = 4;
static constexpr quint32 TABLE_KEYS            = 5;
static constexpr quint32 TABLE_COLORS          = 6;
static constexpr quint32 TABLE_PLAYLIST_TREE   = 7;
static constexpr quint32 TABLE_PLAYLIST_ENTRIES = 8;

static constexpr int NUM_TABLES = 9;  // tables 0-8

// ─────────────────────────────────────────────────────────────────────────────
// Helper: write a DeviceSQL short ASCII string into a buffer.
//
// Short ASCII format (rekordbox_pdb.ksy: device_sql_string, kind=isomorphic):
//   byte 0 = (total_byte_count) * 2 + 1   (odd value signals "short ASCII")
//   bytes 1..N = ASCII text
// where total_byte_count = 1 (header) + len(text)
// ─────────────────────────────────────────────────────────────────────────────

static QByteArray encodeDeviceSqlString(const QString& str)
{
    QByteArray ascii = str.toLatin1();
    // Clamp to 126 bytes so the length byte stays within a single byte
    if (ascii.size() > 126)
        ascii.truncate(126);

    int totalLen = 1 + ascii.size();  // 1 header byte + text bytes
    QByteArray result;
    result.reserve(totalLen);
    result.append(static_cast<char>(totalLen * 2 + 1));  // length_and_kind (odd = short ASCII)
    result.append(ascii);
    return result;
}

// ─────────────────────────────────────────────────────────────────────────────
// Helper: compute available heap space for rows on a page, accounting for
// the row-group index that grows backwards from the end.
// ─────────────────────────────────────────────────────────────────────────────

static int availableHeapSpace(int numRowsSoFar)
{
    int numGroups = (numRowsSoFar / ROWS_PER_GROUP) + 1;  // at least 1 group
    int indexSize = numGroups * ROW_GROUP_SIZE;
    int available = static_cast<int>(PAGE_SIZE) - static_cast<int>(PAGE_HEAP_OFFSET) - indexSize;
    return available;
}

// ─────────────────────────────────────────────────────────────────────────────
// Per-page accumulator: collects serialised rows and their heap offsets,
// then writes a complete 4096-byte page.
// ─────────────────────────────────────────────────────────────────────────────

struct PageBuilder {
    quint32 pageType    = 0;
    quint32 pageIndex   = 0;
    quint32 nextPage    = 0;  // filled in later when linking pages

    QByteArray heapData;      // row data concatenated
    std::vector<quint16> rowOffsets;  // offset of each row within heapData

    /// Try to append a serialised row. Returns false if it won't fit.
    bool tryAddRow(const QByteArray& rowBytes)
    {
        int rowsAfter = static_cast<int>(rowOffsets.size()) + 1;
        int groupsNeeded = ((rowsAfter - 1) / ROWS_PER_GROUP) + 1;
        int indexSize = groupsNeeded * ROW_GROUP_SIZE;
        int heapAfter = heapData.size() + rowBytes.size();
        int total = static_cast<int>(PAGE_HEAP_OFFSET) + heapAfter + indexSize;
        if (total > static_cast<int>(PAGE_SIZE))
            return false;

        quint16 offset = static_cast<quint16>(heapData.size());
        rowOffsets.push_back(offset);
        heapData.append(rowBytes);
        return true;
    }

    /// Serialize the complete 4096-byte page.
    QByteArray serialize() const
    {
        QByteArray page(PAGE_SIZE, '\0');

        // ── Page header (rekordbox_pdb.ksy: page) ──
        // 0x00: 4 bytes gap (zero)
        // 0x04: page_index
        qToLittleEndian<quint32>(pageIndex, reinterpret_cast<uchar*>(page.data() + 0x04));
        // 0x08: type
        qToLittleEndian<quint32>(pageType, reinterpret_cast<uchar*>(page.data() + 0x08));
        // 0x0C: next_page
        qToLittleEndian<quint32>(nextPage, reinterpret_cast<uchar*>(page.data() + 0x0C));
        // 0x10: sequence (1 is fine for a fresh export)
        qToLittleEndian<quint32>(1, reinterpret_cast<uchar*>(page.data() + 0x10));
        // 0x14: 4 bytes gap (zero)

        // 0x18-0x1A: packed num_row_offsets (13 bits) and num_rows (11 bits)
        //   Stored as a little-endian 3-byte value.  Layout from the ksy:
        //   bits [12:0]  = num_row_offsets   (ever-allocated count)
        //   bits [23:13] = num_rows          (valid row count, not shifted)
        //
        //   Rekordbox actually packs this as:
        //     byte 0x18 = low 8 bits of (num_rows_large | (num_rows_small << 13))
        //   But the practical encoding observed in real files and used by
        //   crate-digger is:
        //     u16 at 0x18 (LE) has num_row_offsets in low 13 bits
        //     remaining 3 bits + byte at 0x1A holds num_rows

        quint16 numRowOffsets = static_cast<quint16>(rowOffsets.size());
        quint16 numRows       = numRowOffsets;  // all rows are valid

        // Pack into 3 bytes: bits 0-12 = num_row_offsets, bits 13-23 = num_rows
        quint32 packed = (static_cast<quint32>(numRows) << 13) | numRowOffsets;
        page[0x18] = static_cast<char>(packed & 0xFF);
        page[0x19] = static_cast<char>((packed >> 8) & 0xFF);
        page[0x1A] = static_cast<char>((packed >> 16) & 0xFF);

        // 0x1B: page_flags — 0x34 for normal data pages with rows, 0x24 for empty
        //   (is_data_page = (page_flags & 0x40) == 0)
        page[0x1B] = numRows > 0 ? '\x34' : '\x24';

        // 0x1C-0x1D: free_size (unused heap space)
        int heapCapacity = static_cast<int>(PAGE_SIZE) - static_cast<int>(PAGE_HEAP_OFFSET);
        int groupsNeeded = numRowOffsets > 0
            ? ((numRowOffsets - 1) / ROWS_PER_GROUP) + 1
            : 1;
        int indexSize = groupsNeeded * ROW_GROUP_SIZE;
        quint16 freeSize = static_cast<quint16>(heapCapacity - indexSize - heapData.size());
        qToLittleEndian<quint16>(freeSize, reinterpret_cast<uchar*>(page.data() + 0x1C));

        // 0x1E-0x1F: used_size
        quint16 usedSize = static_cast<quint16>(heapData.size());
        qToLittleEndian<quint16>(usedSize, reinterpret_cast<uchar*>(page.data() + 0x1E));

        // 0x20-0x21: transaction_row_count — 0x1FFF (no transaction)
        qToLittleEndian<quint16>(0x1FFF, reinterpret_cast<uchar*>(page.data() + 0x20));
        // 0x22-0x23: transaction_row_index — 0x1FFF
        qToLittleEndian<quint16>(0x1FFF, reinterpret_cast<uchar*>(page.data() + 0x22));
        // 0x24-0x25: u6 = 0 (data page)
        // 0x26-0x27: u7 = 0

        // ── Heap data ──
        std::memcpy(page.data() + PAGE_HEAP_OFFSET, heapData.constData(), heapData.size());

        // ── Row groups (grow backwards from end of page) ──
        // rekordbox_pdb.ksy: row_group
        //   Each group is 36 bytes and covers up to 16 rows:
        //     u16 row_present_flags   (bit N = row N is present)
        //     u16 unknown             (transaction flags, set to 0)
        //     u16[16] row_offsets     (heap offset for each row, 0 if unused)
        //
        //   Groups are laid out at the END of the page, growing backwards.
        //   Group 0 is at pageEnd - 36, group 1 at pageEnd - 72, etc.

        for (int g = 0; g < groupsNeeded; ++g) {
            int groupBase = static_cast<int>(PAGE_SIZE) - (g + 1) * ROW_GROUP_SIZE;

            // row_present_flags: set bit for each valid row in this group
            quint16 presentFlags = 0;
            int firstRow = g * ROWS_PER_GROUP;
            for (int r = 0; r < ROWS_PER_GROUP; ++r) {
                if (firstRow + r < static_cast<int>(rowOffsets.size()))
                    presentFlags |= (1u << r);
            }
            qToLittleEndian<quint16>(presentFlags, reinterpret_cast<uchar*>(page.data() + groupBase));

            // transaction_row_flags = 0
            qToLittleEndian<quint16>(0, reinterpret_cast<uchar*>(page.data() + groupBase + 2));

            // row offsets (16 x u16)
            for (int r = 0; r < ROWS_PER_GROUP; ++r) {
                quint16 ofs = 0;
                if (firstRow + r < static_cast<int>(rowOffsets.size()))
                    ofs = rowOffsets[firstRow + r];
                qToLittleEndian<quint16>(ofs,
                    reinterpret_cast<uchar*>(page.data() + groupBase + 4 + r * 2));
            }
        }

        return page;
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// Build pages for a table: splits rows across multiple pages as needed.
// Returns the list of pages (with pageIndex not yet assigned).
// ─────────────────────────────────────────────────────────────────────────────

static std::vector<PageBuilder> buildTablePages(quint32 tableType,
                                                 const std::vector<QByteArray>& rows)
{
    std::vector<PageBuilder> pages;

    PageBuilder current;
    current.pageType = tableType;

    for (const auto& row : rows) {
        if (!current.tryAddRow(row)) {
            // Current page is full — finalize and start a new one
            pages.push_back(std::move(current));
            current = PageBuilder();
            current.pageType = tableType;

            if (!current.tryAddRow(row)) {
                qWarning() << "PdbWriter: row too large for a single page, skipping";
                continue;
            }
        }
    }

    // Always emit at least one page (even if empty) so the table has a valid page
    pages.push_back(std::move(current));
    return pages;
}

// ─────────────────────────────────────────────────────────────────────────────
// Row serializers
// ─────────────────────────────────────────────────────────────────────────────

/// Serialize a genre_row (rekordbox_pdb.ksy: genre_row).
///   +0x00  u32  id
///   +0x04  device_sql_string  name
static QByteArray serializeGenreRow(quint32 id, const QString& name)
{
    QByteArray nameBytes = encodeDeviceSqlString(name);
    QByteArray row(4, '\0');
    qToLittleEndian<quint32>(id, reinterpret_cast<uchar*>(row.data()));
    row.append(nameBytes);
    return row;
}

/// Serialize an artist_row with subtype 0x60 (near offset variant).
/// rekordbox_pdb.ksy: artist_row
///   +0x00  u16  subtype       (0x60)
///   +0x02  u16  index_shift   (row index << 5, or 0)
///   +0x04  u32  id
///   +0x08  u8   0x03
///   +0x09  u8   ofs_name_near (offset from row start to the name string)
static QByteArray serializeArtistRow(quint32 id, const QString& name)
{
    QByteArray nameBytes = encodeDeviceSqlString(name);

    QByteArray row(10, '\0');
    // subtype = 0x60
    qToLittleEndian<quint16>(0x0060, reinterpret_cast<uchar*>(row.data() + 0x00));
    // index_shift = 0
    qToLittleEndian<quint16>(0x0000, reinterpret_cast<uchar*>(row.data() + 0x02));
    // id
    qToLittleEndian<quint32>(id, reinterpret_cast<uchar*>(row.data() + 0x04));
    // constant 0x03
    row[0x08] = '\x03';
    // ofs_name_near = offset from row start to the name string = 10 (0x0A)
    row[0x09] = static_cast<char>(10);

    row.append(nameBytes);
    return row;
}

/// Serialize an album_row with subtype 0x80 (near offset variant).
/// rekordbox_pdb.ksy: album_row
///   +0x00  u16  subtype         (0x80)
///   +0x02  u16  index_shift
///   +0x04  u32  unknown         (0)
///   +0x08  u32  artist_id
///   +0x0C  u32  id
///   +0x10  u32  unknown3        (0)
///   +0x14  u8   0x03
///   +0x15  u8   ofs_name_near
static QByteArray serializeAlbumRow(quint32 id, quint32 artistId, const QString& name)
{
    QByteArray nameBytes = encodeDeviceSqlString(name);

    QByteArray row(22, '\0');  // 0x16 = 22 bytes fixed prefix
    // subtype = 0x80
    qToLittleEndian<quint16>(0x0080, reinterpret_cast<uchar*>(row.data() + 0x00));
    // index_shift = 0
    qToLittleEndian<quint16>(0x0000, reinterpret_cast<uchar*>(row.data() + 0x02));
    // unknown = 0
    // artist_id
    qToLittleEndian<quint32>(artistId, reinterpret_cast<uchar*>(row.data() + 0x08));
    // id
    qToLittleEndian<quint32>(id, reinterpret_cast<uchar*>(row.data() + 0x0C));
    // unknown3 = 0
    // 0x03
    row[0x14] = '\x03';
    // ofs_name_near = 22 (0x16)
    row[0x15] = static_cast<char>(22);

    row.append(nameBytes);
    return row;
}

/// Serialize a playlist_tree_row (rekordbox_pdb.ksy: playlist_tree_row).
///   +0x00  u32  parent_id
///   +0x04  u32  gap (0)
///   +0x08  u32  sort_order
///   +0x0C  u32  id
///   +0x10  u32  raw_is_folder (0 = playlist, nonzero = folder)
///   +0x14  device_sql_string  name
static QByteArray serializePlaylistTreeRow(quint32 id, quint32 parentId,
                                            quint32 sortOrder, bool isFolder,
                                            const QString& name)
{
    QByteArray nameBytes = encodeDeviceSqlString(name);

    QByteArray row(20, '\0');  // 0x14 = 20 bytes fixed
    qToLittleEndian<quint32>(parentId, reinterpret_cast<uchar*>(row.data() + 0x00));
    // gap = 0
    qToLittleEndian<quint32>(sortOrder, reinterpret_cast<uchar*>(row.data() + 0x08));
    qToLittleEndian<quint32>(id, reinterpret_cast<uchar*>(row.data() + 0x0C));
    qToLittleEndian<quint32>(isFolder ? 1u : 0u, reinterpret_cast<uchar*>(row.data() + 0x10));
    row.append(nameBytes);
    return row;
}

/// Serialize a playlist_entry_row (rekordbox_pdb.ksy: playlist_entry_row).
///   +0x00  u32  entry_index  (1-based position within playlist)
///   +0x04  u32  track_id
///   +0x08  u32  playlist_id
static QByteArray serializePlaylistEntryRow(quint32 entryIndex, quint32 trackId,
                                             quint32 playlistId)
{
    QByteArray row(12, '\0');
    qToLittleEndian<quint32>(entryIndex, reinterpret_cast<uchar*>(row.data() + 0x00));
    qToLittleEndian<quint32>(trackId,    reinterpret_cast<uchar*>(row.data() + 0x04));
    qToLittleEndian<quint32>(playlistId, reinterpret_cast<uchar*>(row.data() + 0x08));
    return row;
}

/// Map a format string to the Pioneer file_type byte.
/// rekordbox_pdb.ksy: not in ksy, but documented in the exports page:
///   0x00 = unknown, 0x01 = MP3, 0x04 = M4A/AAC, 0x05 = FLAC,
///   0x0B = WAV, 0x0C = AIFF
static quint8 formatToFileType(const std::string& fmt)
{
    if (fmt == "mp3")  return 0x01;
    if (fmt == "m4a" || fmt == "aac" || fmt == "mp4") return 0x04;
    if (fmt == "flac") return 0x05;
    if (fmt == "wav")  return 0x0B;
    if (fmt == "aiff" || fmt == "aif") return 0x0C;
    return 0x00;
}

/// Parse "M:SS" duration string to total seconds.
static quint16 parseDuration(const std::string& time)
{
    QString ts = QString::fromStdString(time);
    QStringList parts = ts.split(QLatin1Char(':'));
    if (parts.size() == 2)
        return static_cast<quint16>(parts[0].toInt() * 60 + parts[1].toInt());
    return 0;
}

/// Serialize a track_row (rekordbox_pdb.ksy: track_row).
///
/// Fixed prefix: 0x5E bytes (94 bytes) up to the string offset array,
/// then 21 x u16 string offsets, then the actual string data.
///
/// The string offsets are relative to the START of the row.
static QByteArray serializeTrackRow(const Track& track,
                                     quint32 genreId, quint32 artistId,
                                     quint32 albumId)
{
    // Build all 21 strings. Most are empty; we only populate the ones we have.
    // Indices per rekordbox_pdb.ksy: track_row.ofs_strings
    //  0=isrc, 1=texter, 2=unknown_2, 3=unknown_3, 4=unknown_4,
    //  5=message, 6=kuvo_public, 7=autoload_hotcues, 8=unknown_5, 9=unknown_6,
    // 10=date_added, 11=release_date, 12=mix_name, 13=unknown_7,
    // 14=analyze_path, 15=analyze_date, 16=comment, 17=title,
    // 18=unknown_8, 19=filename, 20=file_path

    QString strings[21];
    strings[10] = QString::fromStdString(track.date_added);
    strings[16] = QString::fromStdString(track.comment);
    strings[17] = QString::fromStdString(track.title);

    // Extract filename from filepath
    QString fullPath = QString::fromStdString(track.filepath);
    int lastSlash = fullPath.lastIndexOf(QLatin1Char('/'));
    if (lastSlash < 0)
        lastSlash = fullPath.lastIndexOf(QLatin1Char('\\'));
    strings[19] = (lastSlash >= 0) ? fullPath.mid(lastSlash + 1) : fullPath;
    strings[20] = fullPath;

    // Analyze path: just use the filepath (CDJs will look for ANLZ files based on this)
    strings[14] = fullPath;

    // Encode all strings
    QByteArray encodedStrings[21];
    for (int i = 0; i < 21; ++i)
        encodedStrings[i] = encodeDeviceSqlString(strings[i]);

    // Fixed prefix size: 0x5E (94) bytes before string offset array starts
    // String offset array: 21 x 2 = 42 bytes
    // Total fixed area: 94 + 42 = 136 bytes, but wait --
    //   Looking more carefully at the ksy:
    //   0x00-0x5D = 94 bytes of fixed fields
    //   0x5E      = start of ofs_strings[21] = 42 bytes
    //   0x88      = start of string data area
    static constexpr int FIXED_SIZE = 0x5E;            // 94 bytes
    static constexpr int OFS_ARRAY_SIZE = 21 * 2;      // 42 bytes
    static constexpr int HEADER_SIZE = FIXED_SIZE + OFS_ARRAY_SIZE;  // 136 bytes

    // Calculate string data total size
    int stringDataSize = 0;
    for (int i = 0; i < 21; ++i)
        stringDataSize += encodedStrings[i].size();

    QByteArray row(HEADER_SIZE + stringDataSize, '\0');

    // ── Fixed fields ──

    // +0x00: subtype = 0x0024 (8-bit string offsets, basic layout)
    qToLittleEndian<quint16>(0x0024, reinterpret_cast<uchar*>(row.data() + 0x00));
    // +0x02: index_shift = 0
    qToLittleEndian<quint16>(0x0000, reinterpret_cast<uchar*>(row.data() + 0x02));
    // +0x04: bitmask = 0
    qToLittleEndian<quint32>(0, reinterpret_cast<uchar*>(row.data() + 0x04));
    // +0x08: sample_rate = 44100
    qToLittleEndian<quint32>(44100, reinterpret_cast<uchar*>(row.data() + 0x08));
    // +0x0C: composer_id = 0
    // +0x10: file_size = 0 (unknown)
    // +0x14: unknown = 0
    // +0x18: u3 = 0x4A48
    qToLittleEndian<quint16>(0x4A48, reinterpret_cast<uchar*>(row.data() + 0x18));
    // +0x1A: u4 = 0x78F7
    qToLittleEndian<quint16>(0x78F7, reinterpret_cast<uchar*>(row.data() + 0x1A));
    // +0x1C: artwork_id = 0
    // +0x20: key_id = 0
    // +0x24: original_artist_id = 0
    // +0x28: label_id = 0
    // +0x2C: remixer_id = 0
    // +0x30: bitrate
    qToLittleEndian<quint32>(static_cast<quint32>(track.bitrate), reinterpret_cast<uchar*>(row.data() + 0x30));
    // +0x34: track_number = 0
    // +0x38: tempo (BPM * 100)
    quint32 tempo = static_cast<quint32>(track.bpm * 100.0);
    qToLittleEndian<quint32>(tempo, reinterpret_cast<uchar*>(row.data() + 0x38));
    // +0x3C: genre_id
    qToLittleEndian<quint32>(genreId, reinterpret_cast<uchar*>(row.data() + 0x3C));
    // +0x40: album_id
    qToLittleEndian<quint32>(albumId, reinterpret_cast<uchar*>(row.data() + 0x40));
    // +0x44: artist_id
    qToLittleEndian<quint32>(artistId, reinterpret_cast<uchar*>(row.data() + 0x44));
    // +0x48: id
    qToLittleEndian<quint32>(static_cast<quint32>(track.id), reinterpret_cast<uchar*>(row.data() + 0x48));
    // +0x4C: disc_number = 0
    // +0x4E: play_count
    qToLittleEndian<quint16>(static_cast<quint16>(track.play_count), reinterpret_cast<uchar*>(row.data() + 0x4E));
    // +0x50: year = 0
    // +0x52: sample_depth = 16
    qToLittleEndian<quint16>(16, reinterpret_cast<uchar*>(row.data() + 0x52));
    // +0x54: duration (seconds)
    qToLittleEndian<quint16>(parseDuration(track.time), reinterpret_cast<uchar*>(row.data() + 0x54));
    // +0x56: u5 = 0x29
    qToLittleEndian<quint16>(0x0029, reinterpret_cast<uchar*>(row.data() + 0x56));
    // +0x58: color_id
    row[0x58] = static_cast<char>(track.color_label);
    // +0x59: rating (0-5)
    row[0x59] = static_cast<char>(track.rating);
    // +0x5A: file_type
    row[0x5A] = static_cast<char>(formatToFileType(track.format));
    // +0x5B: padding/unknown = 0
    // +0x5C: u7 = 0x0003
    qToLittleEndian<quint16>(0x0003, reinterpret_cast<uchar*>(row.data() + 0x5C));

    // ── String offset array (21 x u16) at 0x5E ──
    // Each offset is relative to the row start. The string data begins
    // right after the offset array at position HEADER_SIZE.
    int stringPos = HEADER_SIZE;
    for (int i = 0; i < 21; ++i) {
        qToLittleEndian<quint16>(static_cast<quint16>(stringPos),
            reinterpret_cast<uchar*>(row.data() + FIXED_SIZE + i * 2));
        // Copy the encoded string data at the current position
        std::memcpy(row.data() + stringPos, encodedStrings[i].constData(),
                    encodedStrings[i].size());
        stringPos += encodedStrings[i].size();
    }

    return row;
}

// ─────────────────────────────────────────────────────────────────────────────
// Main writer
// ─────────────────────────────────────────────────────────────────────────────

QString PdbWriter::write(const QString& path,
                          const QVector<Playlist>& playlists,
                          const QMap<long long, QVector<Track>>& playlistTracks,
                          const QVector<Track>& allTracks,
                          const QMap<long long, QVector<CuePoint>>& /*cueMap*/)
{
    // ── Step 1: Build lookup tables for genres, artists, albums ──
    // Assign sequential IDs starting from 1.

    QMap<QString, quint32> genreMap;   // genre name -> PDB id
    QMap<QString, quint32> artistMap;  // artist name -> PDB id
    // album key = "artist|||album" to handle same album name by different artists
    QMap<QString, quint32> albumMap;
    QMap<QString, quint32> albumArtistMap;  // album key -> artist PDB id

    quint32 nextGenreId  = 1;
    quint32 nextArtistId = 1;
    quint32 nextAlbumId  = 1;

    for (const auto& t : allTracks) {
        QString genre  = QString::fromStdString(t.genre);
        QString artist = QString::fromStdString(t.artist);
        QString album  = QString::fromStdString(t.album);

        if (!genre.isEmpty() && !genreMap.contains(genre))
            genreMap[genre] = nextGenreId++;

        if (!artist.isEmpty() && !artistMap.contains(artist))
            artistMap[artist] = nextArtistId++;

        if (!album.isEmpty()) {
            QString albumKey = artist.toLower() + QStringLiteral("|||") + album.toLower();
            if (!albumMap.contains(albumKey)) {
                albumMap[albumKey] = nextAlbumId++;
                albumArtistMap[albumKey] = artistMap.value(artist, 0);
            }
        }
    }

    // ── Step 2: Serialize rows for each table ──

    // Genre rows
    std::vector<QByteArray> genreRows;
    for (auto it = genreMap.constBegin(); it != genreMap.constEnd(); ++it)
        genreRows.push_back(serializeGenreRow(it.value(), it.key()));

    // Artist rows
    std::vector<QByteArray> artistRows;
    for (auto it = artistMap.constBegin(); it != artistMap.constEnd(); ++it)
        artistRows.push_back(serializeArtistRow(it.value(), it.key()));

    // Album rows
    std::vector<QByteArray> albumRows;
    for (auto it = albumMap.constBegin(); it != albumMap.constEnd(); ++it) {
        quint32 artId = albumArtistMap.value(it.key(), 0);
        // Recover the original-case album name from the key
        // The key is "artist|||album" lowercased, so we need to find the original
        // We'll build a separate map for display names
        QString displayName;
        // Search tracks for this album key to get original casing
        for (const auto& t : allTracks) {
            QString ak = QString::fromStdString(t.artist).toLower()
                       + QStringLiteral("|||")
                       + QString::fromStdString(t.album).toLower();
            if (ak == it.key()) {
                displayName = QString::fromStdString(t.album);
                break;
            }
        }
        albumRows.push_back(serializeAlbumRow(it.value(), artId, displayName));
    }

    // Track rows
    std::vector<QByteArray> trackRows;
    for (const auto& t : allTracks) {
        quint32 gid = genreMap.value(QString::fromStdString(t.genre), 0);
        quint32 aid = artistMap.value(QString::fromStdString(t.artist), 0);
        QString albumKey = QString::fromStdString(t.artist).toLower()
                         + QStringLiteral("|||")
                         + QString::fromStdString(t.album).toLower();
        quint32 lid = albumMap.value(albumKey, 0);
        trackRows.push_back(serializeTrackRow(t, gid, aid, lid));
    }

    // Playlist tree rows:
    //   First, a root folder (id=0, parent_id=0, is_folder=true, name="ROOT")
    //   Then one entry per playlist (parent_id=0, is_folder=false)
    std::vector<QByteArray> playlistTreeRows;
    playlistTreeRows.push_back(
        serializePlaylistTreeRow(0, 0, 0, true, QStringLiteral("ROOT")));

    quint32 sortOrder = 1;
    // Build a mapping from original playlist ID to PDB playlist ID
    QMap<long long, quint32> playlistIdMap;
    quint32 nextPlId = 1;
    for (const auto& pl : playlists) {
        playlistIdMap[pl.id] = nextPlId;
        playlistTreeRows.push_back(
            serializePlaylistTreeRow(nextPlId, 0, sortOrder++, false,
                                     QString::fromStdString(pl.name)));
        ++nextPlId;
    }

    // Playlist entry rows
    std::vector<QByteArray> playlistEntryRows;
    for (const auto& pl : playlists) {
        quint32 pdbPlId = playlistIdMap.value(pl.id, 0);
        const auto& tracks = playlistTracks.value(pl.id);
        quint32 entryIdx = 1;
        for (const auto& t : tracks) {
            playlistEntryRows.push_back(
                serializePlaylistEntryRow(entryIdx++, static_cast<quint32>(t.id), pdbPlId));
        }
    }

    // ── Step 3: Build pages for each table ──

    auto trackPages   = buildTablePages(TABLE_TRACKS,          trackRows);
    auto genrePages   = buildTablePages(TABLE_GENRES,          genreRows);
    auto artistPages  = buildTablePages(TABLE_ARTISTS,         artistRows);
    auto albumPages   = buildTablePages(TABLE_ALBUMS,          albumRows);
    auto labelPages   = buildTablePages(TABLE_LABELS,          {});  // empty
    auto keyPages     = buildTablePages(TABLE_KEYS,            {});  // empty
    auto colorPages   = buildTablePages(TABLE_COLORS,          {});  // empty
    auto plTreePages  = buildTablePages(TABLE_PLAYLIST_TREE,   playlistTreeRows);
    auto plEntryPages = buildTablePages(TABLE_PLAYLIST_ENTRIES, playlistEntryRows);

    // Collect all table page vectors in table-type order
    std::vector<std::vector<PageBuilder>*> allTablePages = {
        &trackPages, &genrePages, &artistPages, &albumPages,
        &labelPages, &keyPages, &colorPages, &plTreePages, &plEntryPages
    };

    // ── Step 4: Assign page indices and link pages ──
    // Page 0 = file header page. Data pages start from page 1.

    quint32 nextPageIndex = 1;

    // For each table, record first_page and last_page indices
    struct TableInfo {
        quint32 type       = 0;
        quint32 firstPage  = 0;
        quint32 lastPage   = 0;
    };
    std::vector<TableInfo> tableInfos(NUM_TABLES);

    for (int t = 0; t < NUM_TABLES; ++t) {
        auto& pages = *allTablePages[t];
        tableInfos[t].type = static_cast<quint32>(t);
        tableInfos[t].firstPage = nextPageIndex;

        for (size_t i = 0; i < pages.size(); ++i) {
            pages[i].pageIndex = nextPageIndex++;
            // Link to next page in the same table, or to self for the last page
            if (i + 1 < pages.size())
                pages[i].nextPage = nextPageIndex;  // will be assigned next iteration
            else
                pages[i].nextPage = pages[i].pageIndex;  // last page points to itself
        }

        tableInfos[t].lastPage = pages.back().pageIndex;
    }

    quint32 totalPages = nextPageIndex;  // includes page 0

    // ── Step 5: Build the file header (page 0) ──
    // rekordbox_pdb.ksy: rekordbox_pdb (top level)
    //   +0x00  u32  unknown (0)
    //   +0x04  u32  len_page (4096)
    //   +0x08  u32  num_tables
    //   +0x0C  u32  next_unused_page (= totalPages)
    //   +0x10  u32  unknown (0)
    //   +0x14  u32  sequence (1)
    //   +0x18  4 bytes gap (0)
    //   +0x1C  table_pointer[num_tables], each 16 bytes:
    //          +0x00 u32 type
    //          +0x04 u32 empty_candidate (0)
    //          +0x08 u32 first_page
    //          +0x0C u32 last_page

    QByteArray headerPage(PAGE_SIZE, '\0');
    // unknown = 0 (already zero)
    qToLittleEndian<quint32>(PAGE_SIZE, reinterpret_cast<uchar*>(headerPage.data() + 0x04));
    qToLittleEndian<quint32>(static_cast<quint32>(NUM_TABLES),
                              reinterpret_cast<uchar*>(headerPage.data() + 0x08));
    qToLittleEndian<quint32>(totalPages, reinterpret_cast<uchar*>(headerPage.data() + 0x0C));
    // 0x10 = 0
    qToLittleEndian<quint32>(1, reinterpret_cast<uchar*>(headerPage.data() + 0x14));  // sequence
    // 0x18 gap = 0

    // Table pointers at 0x1C
    for (int t = 0; t < NUM_TABLES; ++t) {
        int base = 0x1C + t * 16;
        qToLittleEndian<quint32>(tableInfos[t].type,
            reinterpret_cast<uchar*>(headerPage.data() + base + 0x00));
        // empty_candidate = 0
        qToLittleEndian<quint32>(tableInfos[t].firstPage,
            reinterpret_cast<uchar*>(headerPage.data() + base + 0x08));
        qToLittleEndian<quint32>(tableInfos[t].lastPage,
            reinterpret_cast<uchar*>(headerPage.data() + base + 0x0C));
    }

    // ── Step 6: Write the file ──

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return QStringLiteral("Cannot open PDB file for writing: ") + path;

    // Write page 0 (header)
    if (file.write(headerPage) != PAGE_SIZE) {
        file.close();
        return QStringLiteral("Failed to write PDB header page");
    }

    // Write data pages in order
    for (int t = 0; t < NUM_TABLES; ++t) {
        for (auto& pg : *allTablePages[t]) {
            QByteArray pageData = pg.serialize();
            if (file.write(pageData) != PAGE_SIZE) {
                file.close();
                return QStringLiteral("Failed to write PDB data page");
            }
        }
    }

    file.close();
    if (file.error() != QFile::NoError)
        return QStringLiteral("PDB write error: ") + file.errorString();

    qInfo() << "PdbWriter: wrote" << totalPages << "pages ("
            << allTracks.size() << "tracks,"
            << playlists.size() << "playlists ) to" << path;

    return {};
}
