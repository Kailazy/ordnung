#pragma once

// =============================================================================
// Theme.h — Single source of truth for all design tokens.
//
// Aesthetic: Ableton-inspired, machinelike. Warmer charcoal greys, orange
// accent, sharp geometry, monospace-heavy. Functional, precise, no decoration.
//
// Rules:
//   - Every color, font size, and layout constant in the application must
//     trace back to a named token here.
//   - theme.qss references these tokens by name in comments.
//   - Delegates and paint code read from here — never hardcode hex values.
//   - The constexpr const char* color tokens can be passed directly to
//     QColor(), e.g. QColor(Theme::Color::Accent).
// =============================================================================

#include <QColor>
#include <QString>

namespace Theme {

// ─────────────────────────────────────────────────────────────────────────────
// Color — Ableton-inspired: charcoal greys, orange accent, machinelike
// ─────────────────────────────────────────────────────────────────────────────
namespace Color {

    // Surfaces (darkest → lightest) — warmer charcoal, not pure black
    constexpr auto Bg  = "#1a1a1a";   // root/window background
    constexpr auto Bg1 = "#222222";   // sidebar, playlist panel, secondary surfaces
    constexpr auto Bg2 = "#2a2a2a";   // cards, inputs, interactive surface base
    constexpr auto Bg3 = "#353535";   // hover surface offset, elevated state

    // Borders — precise, structural
    constexpr auto Border        = "#2d2d2d";   // structural dividers
    constexpr auto BorderHov     = "#505050";   // hover/active border elevation
    constexpr auto RowSeparator  = "#353535";   // table row/column lines, slightly stronger

    // Text hierarchy
    constexpr auto Text       = "#d8d8d8";  // primary, readable
    constexpr auto TextBright = "#f0f0f0";  // track title (slightly elevated)
    constexpr auto Text2      = "#808080";  // secondary, metadata
    constexpr auto Text3      = "#5a5a5a";  // muted, labels, inactive

    // Accent — Ableton orange, machinelike
    constexpr auto Accent   = "#ff764d";   // Ableton orange
    constexpr auto AccentBg = "#4a2a22";   // accent wash: row selection, active

    // Row backgrounds for QPainter delegates (must match QSS table rules)
    constexpr auto RowSelHov  = "#5a3228";  // selected + hovered
    constexpr auto RowHov     = "#252525";  // hovered only (subtle lift from Bg)
    constexpr auto RowExpanded = "#1e2020"; // expanded (no selection) — very subtle teal tint

    // Semantic colors
    constexpr auto Green   = "#7cb87e";    // AIFF (target format), success
    constexpr auto GreenBg = "#1e2e1e";
    constexpr auto Red     = "#e07070";    // danger, error, failed
    constexpr auto RedBg   = "#3a1e1e";
    constexpr auto Amber   = "#e8a84a";    // warning, pending
    constexpr auto AmberBg = "#3a2a18";
    constexpr auto Gold    = "#b8955a";    // star ratings

    // Pioneer CDJ color labels (indices 1-8, 0 = none)
    inline QColor labelColor(int index) {
        switch (index) {
        case 1: return QColor("#FF007F");  // Pink
        case 2: return QColor("#FF4040");  // Red
        case 3: return QColor("#FF8C00");  // Orange
        case 4: return QColor("#E8D44A");  // Yellow
        case 5: return QColor("#5AC878");  // Green
        case 6: return QColor("#4AC8C8");  // Aqua
        case 7: return QColor("#5078E0");  // Blue
        case 8: return QColor("#9060D0");  // Purple
        default: return QColor(0, 0, 0, 0); // transparent (none)
        }
    }

    // Format-identity colors — unique per audio format
    constexpr auto FmtWav    = "#c88dd0";  constexpr auto FmtWavBg  = "#2a1a30";
    constexpr auto FmtM4a    = "#e88aa0";  constexpr auto FmtM4aBg  = "#3a1522";
    constexpr auto FmtAlac   = "#5ab5ac";  constexpr auto FmtAlacBg = "#0d2e2e";
    constexpr auto FmtAac    = "#9ec86e";  constexpr auto FmtAacBg  = "#1a2e0d";

} // namespace Color


// ─────────────────────────────────────────────────────────────────────────────
// Font — Professional scale (Material/Fluent/Atlassian aligned)
// Body 14pt, Secondary 13pt, Caption 12pt, Small 11pt
// ─────────────────────────────────────────────────────────────────────────────
namespace Font {

    constexpr int Logo      = 18;  // logo wordmark (compact, not oversized)
    constexpr int Title     = 16;  // section titles, page headers
    constexpr int Body      = 14;  // primary body (track titles, playlist names)
    constexpr int Secondary = 13;  // secondary metadata, button text
    constexpr int Caption   = 12;  // labels, section headers, stats, mono data
    constexpr int Meta      = 12;  // alias for Caption (BPM, key, duration)
    constexpr int Small     = 11;  // badges, captions, overline
    constexpr int Badge     = 11;  // format/status badge (alias for Small)

    constexpr auto Mono         = "Consolas";
    constexpr auto MonoFallback = "Courier New";

} // namespace Font


// ─────────────────────────────────────────────────────────────────────────────
// Layout — 8px base unit. All padding/spacing uses these tokens.
// QSS must use matching values (see theme.qss header for reference).
// ─────────────────────────────────────────────────────────────────────────────
namespace Layout {

    constexpr int WindowMinW = 1100;
    constexpr int WindowMinH = 700;

    constexpr int SidebarW  = 224;   // 28×8
    constexpr int PlaylistW = 320;   // 40×8

    constexpr int ToolbarH   = 56;   // 7×8
    constexpr int GenreBarH  = 40;   // 5×8
    constexpr int HeaderH    = 32;   // 4×8 — QHeaderView row height

    constexpr int TrackRowH    = 48;   // 6×8
    constexpr int TrackThumbSize = 36; // 4.5×8 — album art / placeholder in title cell
    constexpr int TrackThumbPad = 8;   // 1× — padding from left edge of cell
    constexpr int DownloadRowH = 44;   // 5.5×8
    constexpr int PlaylistRowH = 56;   // 7×8

    constexpr int DetailPanelMinH  = 120;  // 15×8
    constexpr int DetailPanelMaxH  = 272;  // 34×8
    constexpr int ActivityLogMinH  = 160;  // 20×8
    constexpr int ActivityLogMaxH  = 240;  // 30×8

    // ═══ 8px GRID SCALE — base units ═══
    constexpr int PadXs  = 4;    // 0.5× — icon-text, micro gaps
    constexpr int PadSm  = 8;    // 1× — tight padding, input vertical
    constexpr int PadMd  = 12;   // 1.5× — cell padding, standard gap
    constexpr int PadLg  = 16;   // 2× — comfortable padding
    constexpr int PadXl  = 24;   // 3× — panel/section padding
    constexpr int Pad2Xl = 32;   // 4× — large panels, cards

    // ═══ SEMANTIC PADDING — use these, not raw scale ═══
    constexpr int PanelPad     = PadXl;   // 24 — sidebar, headers, content areas
    constexpr int ContentPadH  = PadXl;  // 24 — horizontal content margins
    constexpr int ContentPadV  = PadMd;  // 12 — vertical content margins
    constexpr int CellPadH     = PadMd;  // 12 — table cells, list items
    constexpr int InputPadV    = PadSm;  // 8 — input/button vertical
    constexpr int InputPadH    = PadMd;  // 12 — input horizontal (or PadLg 16)
    constexpr int ButtonPadV   = PadSm;  // 8
    constexpr int ButtonPadH   = PadLg;  // 16
    constexpr int CardPadH     = Pad2Xl; // 32 — folder nodes, cards
    constexpr int CardPadV     = PadXl;  // 24 — folder nodes top
    constexpr int CardPadVB    = PadLg;  // 16 — folder nodes bottom (slightly less)

    // ═══ GAPS — spacing between elements ═══
    constexpr int GapXs = PadXs;  // 4 — chips, tight groups
    constexpr int GapSm = PadSm;  // 8 — related elements
    constexpr int GapMd = PadMd;  // 12 — standard inter-element
    constexpr int GapLg = PadLg;  // 16 — section separation

    // ═══ LEGACY ALIASES (prefer semantic names above) ═══
    constexpr int Pad      = PanelPad;
    constexpr int Gap      = GapMd;
    constexpr int SpaceXs  = PadXs;
    constexpr int SpaceSm  = PadSm;
    constexpr int SpaceMd  = PadMd;
    constexpr int SpaceLg  = PadLg;
    constexpr int SpaceXl  = PadXl;
    constexpr int Space2Xl = Pad2Xl;
    constexpr int TrackCellPadH = CellPadH;

} // namespace Layout


// ─────────────────────────────────────────────────────────────────────────────
// Badge — compact, professional
// ─────────────────────────────────────────────────────────────────────────────
namespace Badge {

    constexpr int HPad   = 8;   // horizontal padding (Layout::PadSm) — 8px grid
    constexpr int Height = 20;  // 2.5×8 — comfortable tap target
    constexpr int Radius = 2;   // subtle rounding

    struct Colors {
        QColor text;
        QColor bg;
    };

    // Returns the text/bg colors for a given audio format string
    // (case-insensitive: "AIFF", "aiff", "Aiff" are all valid).
    // Unknown formats fall back to muted grey (Text3 / Bg3).
    Colors forFormat(const QString& format);

    // Returns the text/bg colors for a conversion status string
    // ("pending", "converting", "done", "failed").
    // Unknown statuses fall back to muted grey.
    Colors forStatus(const QString& status);

} // namespace Badge

} // namespace Theme
