# Eyebags Terminal — Qt Widgets Design Specification
Version 1.0 | Author: UI/UX Architecture | For: Qt C++ Implementation

---

## Section 1: Philosophy

The aesthetic principle here is not style — it is discipline. Every visual decision traces back to a single question: does this help the person in front of it do their work faster and with less friction? The person is a DJ or producer. The work is managing hundreds of tracks, finding files, triggering conversions. The environment is dark. The cognitive budget is often already spent.

In Qt Widgets terms: do not fight the toolkit's native painting machinery. The QPalette, QPainter, and QStyle systems exist precisely so that widgets render coherently as a unified surface. Where this theme overrides native drawing (track rows, format badges, status indicators), it does so through `QStyledItemDelegate::paint()` — not by embedding child widgets inside every cell. The distinction matters for performance and for interaction fidelity.

Hierarchy is expressed through contrast ratio and spacing, not through borders and shadows. A QFrame separator should have enough visual weight to orient the eye without announcing itself. Dead space between sections is not wasted — it groups related elements and gives the eye a place to rest between focus shifts. The sidebar is permanent, functional, never decorative. The content area earns its real estate by doing work.

Restraint is not minimalism for its own sake. It is a commitment to keeping the user's attention where it belongs: on the tracks, the format badges, the conversion queue. If the chrome disappears and the data surfaces, the design is succeeding.

---

## Section 2: MainWindow Layout

### Window minimum size

Minimum: 1100px wide, 700px tall. Below this, no layout should collapse or overflow — it should simply stop responding to resize. Set `setMinimumSize(1100, 700)` on the QMainWindow.

### Sidebar

- Width: fixed at 220px. Apply `setFixedWidth(220)` to the sidebar QWidget, not a layout minimum — the splitter should not be able to collapse it.
- Background: `#0e0e0e`
- Right edge: 1px border `#1e1e1e`
- Internal padding: 23px top, 0px horizontal (nav buttons span full width), 0px bottom
- Logo area bottom padding: 32px, followed by 1px border `#1e1e1e` separating logo from nav
- Nav section: `QVBoxLayout`, spacing 0, no margins. Buttons fill width.
- Footer: pinned to bottom via a spacer in the sidebar layout. 16px padding all sides. 1px top border `#1e1e1e`.

### Library splitter

- `QSplitter`, orientation: `Qt::Horizontal`
- Initial sizes: `[320, window_width - 320 - 220]` — set via `setSizes()` after show
- Splitter handle: 1px wide, color `#1e1e1e`. On hover: `#444444`. No drag grip texture.
- Left panel (PlaylistPanel): fixed-width behaviour via `QSplitter::setCollapsible(0, false)` and a reasonable minimum width of 240px, maximum 480px. Default 320px.
- Right panel: no max width, expands to fill.

### Toolbar

- Height: fixed 64px via `setFixedHeight(64)` on the toolbar QWidget
- Background: `#080808`
- Bottom border: 1px `#1e1e1e`
- Internal layout: QHBoxLayout, 28px left/right margins, 18px top/bottom padding, 14px spacing between elements
- Children from left to right: search input (flex/expanding), bulk format QComboBox, bulk apply QPushButton, undo QPushButton (appears only when undo is available), stats QLabel (right-aligned, pinned with spacer)

### Genre bar

- Height: fixed 48px via `setFixedHeight(48)` on the genre bar QWidget
- Background: `#080808`
- Bottom border: 1px `#1e1e1e`
- Internal layout: QHBoxLayout, 28px left/right margins, 12px top/bottom padding, 7px spacing
- Overflow: use a QScrollArea set to horizontal-only, frameless, with `QScrollBar:horizontal` hidden via stylesheet when not needed. Do not wrap tags — keep them on one line and let it scroll.

### Track row height

Target: 54px. Set via `setUniformRowHeights(true)` (required) and `verticalHeader()->setDefaultSectionSize(54)` on the QTableView. Do not allow per-row variable heights — this breaks virtualization. The 54px accommodates two lines of text (title + metadata row) with 13px vertical breathing room.

### Track detail panel

The detail panel appears below the currently selected row. Implement as a second QWidget embedded in the right-side QSplitter below the TrackTableView. Default state: hidden (`setVisible(false)`). On row selection, populate and show. Do not animate. Do not slide. Show and hide are instantaneous — the data change is the event, not the motion.

Height: not fixed. Let the content determine it, with a minimum of 120px and a maximum of 280px. The enclosing QSplitter handle gives the user manual control.

### Activity log height

- Minimum: 160px
- Maximum: 250px
- Default: 200px
- Fixed by QSplitter. The log is always visible in the Downloads view — it is not toggleable. Its presence is a permanent indicator of system health.

---

## Section 3: TrackTableView Column Layout

### Column definitions

| Column index | Role | Width | Resizable | Header text |
|---|---|---|---|---|
| 0 | Expand indicator (chevron) | 20px fixed | No | (empty) |
| 1 | Title + Artist (primary cell) | stretch (fill remaining) | Yes | TRACK |
| 2 | BPM | 72px fixed | No | BPM |
| 3 | Key | 64px fixed | No | KEY |
| 4 | Duration | 72px fixed | No | TIME |
| 5 | Format badge | 80px fixed | No | FORMAT |

Apply: `header->setSectionResizeMode(1, QHeaderView::Stretch)`. All other columns: `QHeaderView::Fixed`.

The title+artist column (index 1) is a single cell. The delegate paints two text elements vertically — title at 17pt `#e8e8e8`, artist at 15pt `#777777` below it with 5px gap. Truncate both with ellipsis when they exceed available width. Use `QFontMetrics::elidedText()` in the delegate.

### Header style

- Background: `#080808`
- Text: 13pt, uppercase, `#444444`, letter-spacing 1px
- Bottom border: 1px `#1e1e1e`
- No vertical sort indicator arrows unless sort is actively applied. When sort is active: use a custom SVG indicator, 8px wide, `#777777`. No platform default triangle.
- Height: set via `header->setFixedHeight(36)`

### Row selection color

Do not use the system selection blue. The selection background is `#0d3b4f` (the accent wash). This is applied via `QTableView { selection-background-color: #0d3b4f; }` in QSS and via `QPalette::Highlight` override in code to ensure it applies in all draw paths:

```cpp
QPalette p = trackTable->palette();
p.setColor(QPalette::Highlight, QColor(0x0d, 0x3b, 0x4f));
p.setColor(QPalette::HighlightedText, QColor(0xd0, 0xd0, 0xd0));
trackTable->setPalette(p);
```

### FormatDelegate::paint() — exact specification

The format badge is drawn entirely by the delegate. No QLabel is embedded in the cell.

```
void FormatDelegate::paint(QPainter* p, const QStyleOptionViewItem& opt, const QModelIndex& idx) const
```

Step-by-step geometry and color:

1. Retrieve the format string from the model: `idx.data(Qt::DisplayRole).toString().toLower()`
2. Look up badge colors from a static map keyed on format string:
   - `"mp3"`:  text `#444444`, bg `#1c1c1c`
   - `"flac"`: text `#4fc3f7`, bg `#0d3b4f`
   - `"wav"`:  text `#ce93d8`, bg `#2a1a33`
   - `"aiff"`: text `#81c784`, bg `#1b3a1b`
   - `"ogg"`:  text `#ffb74d`, bg `#3b2a0d`
   - `"m4a"`:  text `#f48fb1`, bg `#3b1525`
   - `"alac"`: text `#4db6ac`, bg `#0d3b35`
   - `"wma"`:  text `#e57373`, bg `#3b1515`
   - `"aac"`:  text `#aed581`, bg `#1f3a0d`
   - fallback: text `#444444`, bg `#1c1c1c`
3. Set font: `QFont("Consolas", 11)`, weight `QFont::DemiBold`
4. Measure text width: `QFontMetrics(font).horizontalAdvance(formatUpper)`
5. Compute badge rect: centered vertically in `opt.rect`, width = textWidth + 14px padding (7px each side), height = 20px, radius = 3px
6. Horizontally center the badge rect within the column cell
7. `p->setRenderHint(QPainter::Antialiasing, true)`
8. Fill rounded rect with bg color
9. Draw text centered in badge rect with text color, uppercase

The row background before drawing the badge: if the row is selected, fill `opt.rect` with `#0d3b4f` first. If hovered (check `opt.state & QStyle::State_MouseOver`), fill with `#0d0d0d`. Otherwise transparent (inherits table background).

### Expand chevron (column 0)

Paint a Unicode right-pointing triangle `▸` at 12pt, color `#444444`. When the row's detail panel is expanded, paint `▾`, color `#4fc3f7`. Use `idx.data(Qt::UserRole)` to carry the expanded boolean from the model.

---

## Section 4: PlaylistPanel

### Visual weight

The playlist panel is a secondary surface — slightly lighter than root (`#0e0e0e` vs `#080808`), separated from the track area by a 1px border at `#1e1e1e`. It should feel like a sidebar panel, not a content area. It never competes for attention with the track list.

### Header area

- Padding: 23px all sides
- Bottom border: 1px `#1e1e1e`
- Contains: "playlists" QLabel (17pt, weight 400, `#777777`, letter-spacing 1px) + import zone below it with 12px gap

### Import zone

A `QLabel` or a custom `QWidget` that accepts drag-and-drop (`.txt` file drops trigger import). Visual:

- Border: 1px dashed `#1e1e1e`
- Border radius: 5px
- Background: transparent
- Text: "drop rekordbox export", 15pt, `#444444`, center-aligned
- Padding: 18px all sides

Hover state (either cursor hover over the widget, or a drag-enter event):
- Border color changes to `#444444` (no dashed → solid transition — QSS cannot do that. Keep dashed. Only the color changes.)
- Text color changes to `#777777`

Drag-enter active state: border color `#4fc3f7`, text color `#4fc3f7`. Implement by applying a dynamic property and calling `style()->unpolish(widget); style()->polish(widget)`.

### Playlist item height

52px minimum. Set via the delegate's `sizeHint()`. Each item contains two text elements: playlist name (17pt, `#d0d0d0`) and track count (15pt, `#444444`) with 3px gap.

### Delete button visibility

The delete button is a QPushButton with `objectName("deleteBtn")`. It sits on the right of each playlist item row, rendered by the playlist item delegate.

Since QSS cannot transition opacity, implement hover-reveal as a static color swap:

- Idle: text color `transparent`, background `transparent` — the button is invisible but occupies space
- Row hover (QStyle::State_MouseOver on the row): text color `#444444`
- Delete button hover (QStyle::State_MouseOver on the button itself): text color `#e57373`
- Pressed: text color `#c05050`

In a QStyledItemDelegate context, this means the delete button is drawn by the delegate in `paint()` — it is not a real embedded QWidget. The delegate tracks `hoveredRow` and `hoveredDeleteRect` as member variables, updated via `mouseMoveEvent` on the view. Clicking within `hoveredDeleteRect` emits a `deleteRequested(QModelIndex)` signal.

---

## Section 5: Downloads View

### FolderFlow layout

The FolderFlow widget is a single QWidget containing a QHBoxLayout. Children:

1. Source node (`QWidget#folderNodeSrc`)
2. Arrow (a `QLabel` or custom `QWidget` that paints an SVG line with arrowhead)
3. Output node (`QWidget#folderNodeOut`)

The layout is centered horizontally with `Qt::AlignCenter`. The nodes do not stretch — they sit at their natural size in the middle of the available width.

### Folder node geometry

- Background: `#151515`
- Border: 1px solid `#1e1e1e`
- Border radius: 24px
- Min-width: 190px
- Max-width: 280px
- Padding: 26px top, 32px left/right, 22px bottom
- Internal layout: QVBoxLayout, centered, spacing 10px
- Children: folder icon SVG (38x38, source: `#4fc3f7`, output: `#81c784`), label text (13pt uppercase letter-spacing 2px `#777777`), path text (13pt `#444444`, elide at max-width with ellipsis, center-aligned)

Hover state:
- Background: `#1c1c1c`
- Border: `#444444`

Pressed state:
- Background: `#151515`
- Border: `#555555`

The entire node is clickable — connect the `mousePressEvent` to `QFileDialog::getExistingDirectory()`. Set `setCursor(Qt::PointingHandCursor)` on the node widget.

### Arrow between nodes

A `QLabel` containing an SVG-rendered image. The SVG is a horizontal line with an arrowhead, approximately 64px wide by 28px tall. Color: `#444444`. No hover state. Padding: 0px 10px on the QLabel.

Exact SVG:
```xml
<svg viewBox="0 0 48 24" fill="none" stroke="#444444" stroke-width="2"
     stroke-linecap="round" stroke-linejoin="round">
  <line x1="2" y1="12" x2="38" y2="12"/>
  <polyline points="32,6 38,12 32,18"/>
</svg>
```

Render via `QSvgRenderer` and `QPixmap`, or embed as a `QLabel` pixmap. Do not use an animated arrow — no motion.

### Action bar

A QHBoxLayout below the FolderFlow, centered (`Qt::AlignHCenter`), spacing 12px:

1. "save" QPushButton — standard style
2. "scan" QPushButton — standard style
3. "convert all" QPushButton — accent style (`btnStyle="accent"`)
4. Status QLabel (only visible when converter is running) — `#81c784` text on `#1b3a1b` background, 5px/12px padding, radius 4px

Busy state for save/scan/convert buttons: change text to "saving..." / "scanning..." / "queuing..." and set `setEnabled(false)`. No animated progress bar inside the button. The activity log communicates progress — the button only needs to indicate it is busy.

### DownloadTable column layout

| Column | Role | Width | Resizable |
|---|---|---|---|
| 0 | Filename | stretch | Yes |
| 1 | Extension badge | 80px | No |
| 2 | Size (MB) | 80px | No |
| 3 | Conversion status badge | 135px | No |
| 4 | Action button (convert / retry) | 40px | No |

Row height: 46px. Set `verticalHeader()->setDefaultSectionSize(46)`.

The extension badge (column 1) uses the same color mapping as the format badges in the library, rendered by `StatusDelegate::paint()` using identical QPainter logic but without a background pill — just the colored text in monospace at 13pt, uppercase, centered in the cell.

The conversion status badge (column 3) is a full rounded rect badge, same geometry as described for FormatDelegate above but using conv status colors:
- `"pending"`: text `#ffb74d`, bg `#3b2a0d`
- `"converting"`: text `#4fc3f7`, bg `#0d3b4f`
- `"done"`: text `#81c784`, bg `#1b3a1b`
- `"failed"`: text `#e57373`, bg `#3b1515`

Column 4 (action): a small "convert" or "retry" text rendered by the delegate, styled as a text button. On hover (tracked by delegate), show in `#4fc3f7`. On idle, show in `#444444`. On click within the rect, emit `convertRequested(QModelIndex)`.

---

## Section 6: ActivityLog

### Structure

The ActivityLog section consists of two widgets stacked vertically in a QVBoxLayout:

1. Header bar (`QWidget#activityLogHeader`) — fixed height 32px
2. Log body (`QPlainTextEdit#activityLog`) — min-height 160px, max-height 250px

The header bar contains a QHBoxLayout with:
- "activity log" QLabel (`#activityLogTitle`): 13pt, `#444444`, letter-spacing 1.5px, uppercase
- Spacer (0px — the dot follows immediately after the label with 9px margin)
- Pulse dot QLabel (`#logDot`): 7px diameter circle, described below

The header bar and log body together form a section that does not expand beyond 250px + 32px = 282px combined.

### Font and text rendering

- Font: `"Consolas", "Courier New", monospace`, 13pt
- Line height: set via QPlainTextEdit's document default block format. Approximate: `QTextBlockFormat` with `setLineHeight(170, QTextBlockFormat::ProportionalHeight)` applied to the document's default format. This gives approximately 1.7x line spacing.
- Text wrapping: `setLineWrapMode(QPlainTextEdit::NoWrap)` — log lines are single lines, no wrapping, horizontal scroll available but visually hidden (the content is already constrained to short timestamped lines)

### Color scheme for log lines

`QPlainTextEdit` does not support per-line styling via QSS alone. Implement using `QTextCharFormat` applied to each appended line:

- Default: `#444444`
- Lines containing "ERROR": `#e57373`
- Lines containing "WARN" or "WARNING": `#ffb74d`

Apply the format via `QTextCursor` each time a new line is appended:

```cpp
void ActivityLog::appendLine(const QString& line) {
    QTextCursor cursor(logEdit->document());
    cursor.movePosition(QTextCursor::End);

    QTextCharFormat fmt;
    if (line.contains("ERROR", Qt::CaseInsensitive))
        fmt.setForeground(QColor("#e57373"));
    else if (line.contains("WARN", Qt::CaseInsensitive))
        fmt.setForeground(QColor("#ffb74d"));
    else
        fmt.setForeground(QColor("#444444"));

    cursor.insertText(line + "\n", fmt);
    logEdit->verticalScrollBar()->setValue(logEdit->verticalScrollBar()->maximum());

    triggerPulse();
}
```

### Pulse indicator

The `#logDot` QLabel is a 7x7px circle widget. Idle color: `#444444`. When a new line arrives, animate its opacity (not scale — QLabel does not scale its background natively) by using `QPropertyAnimation` targeting a custom `opacity` property via a subclass or by setting the palette's window color between two values:

```cpp
void ActivityLog::triggerPulse() {
    // Set to green
    logDot->setStyleSheet("background-color: #81c784; border-radius: 3px;");

    // Return to grey after 1200ms
    QTimer::singleShot(1200, this, [this]() {
        logDot->setStyleSheet("background-color: #444444; border-radius: 3px;");
    });
}
```

Do not use a QPropertyAnimation for this. The instantaneous green-to-grey transition after 1200ms is semantically correct — it communicates "something just happened" without pretending motion is meaningful. If you want a fade, use `QGraphicsOpacityEffect` with a `QPropertyAnimation` on `opacity` from 1.0 to 0.4 over 1200ms, but the static approach is preferred for its simplicity.

Maximum log line count: 200 lines. When the document exceeds 200 blocks, remove the first block before appending.

---

## Section 7: Typography

All font sizes are in **point units**. No pixel sizes are used anywhere in this specification. Point sizes scale correctly across display DPI; pixel sizes do not.

### Size scale

| Role | Size | Weight | Family | Color |
|---|---|---|---|---|
| Logo wordmark ("eyebags / terminal") | 20pt | Light (300) | System sans | `#ffffff` |
| Primary body text (track titles, playlist names) | 17pt | Regular (400) | System sans | `#d0d0d0` |
| Secondary text (artist names, metadata) | 15pt | Regular (400) | System sans | `#777777` |
| Section headers (uppercase labels) | 13pt | Regular (400) | System sans | `#444444` |
| Mono data (BPM, key, duration) | 13pt | Regular (400) | Consolas | `#777777` |
| Format badges, status badges | 11pt | SemiBold (600) | Consolas | (per badge) |
| Activity log lines | 13pt | Regular (400) | Consolas | `#444444` |
| Detail field keys (uppercase micro labels) | 11pt | Regular (400) | System sans | `#444444` |
| Detail field values | 13pt | Regular (400) | System sans | `#d0d0d0` |
| Button text | 15pt | Regular (400) | System sans | (per button style) |
| Small button text | 13pt | Regular (400) | System sans | (per button style) |

### Font family resolution

In `main.cpp` or `Application::initialize()`:

```cpp
QFont appFont = QApplication::font();
appFont.setPointSize(17);  // base size — widgets override as needed
appFont.setHintingPreference(QFont::PreferFullHinting);
QApplication::setFont(appFont);
```

Do not hardcode "Segoe UI" or "SF Pro" — Qt's platform integration delivers the correct system sans-serif automatically. The only family name that must be hardcoded is `"Consolas", "Courier New", monospace` for all mono elements.

### Letter spacing

QSS `letter-spacing` is not reliably cross-platform in Qt Widgets. Set letter spacing programmatically via `QFont::setLetterSpacing(QFont::AbsoluteSpacing, 3.0)` where indicated (logo, section headers, nav buttons). Do not rely on QSS letter-spacing for these elements.

### Text rendering quality

```cpp
QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
// Qt 6: high-DPI scaling is automatic — no AA_EnableHighDpiScaling needed
```

In all custom `paintEvent` and `QStyledItemDelegate::paint()` implementations:

```cpp
painter->setRenderHint(QPainter::TextAntialiasing, true);
painter->setRenderHint(QPainter::Antialiasing, true);
```

---

## Section 8: Interaction States

QSS supports three pseudo-states relevant to this application: `:hover`, `:pressed`, `:disabled`. There are no transitions. All state changes are instantaneous. The designer accepts this constraint and specifies states accordingly — no visual state requires motion to be legible.

### Nav button

| State | Background | Text color | Left border |
|---|---|---|---|
| Idle | transparent | `#444444` | 2px transparent |
| Hover | `#151515` | `#777777` | 2px transparent |
| Pressed | `#151515` | `#d0d0d0` | 2px transparent |
| Active (current tab) | `#151515` | `#d0d0d0` | 2px `#4fc3f7` |
| Active + Hover | `#1c1c1c` | `#d0d0d0` | 2px `#4fc3f7` |
| Disabled | — (nav buttons are never disabled) | — | — |

### Playlist item (QListView row, via delegate)

| State | Background | Name text | Meta text | Left border |
|---|---|---|---|---|
| Idle | transparent | `#d0d0d0` | `#444444` | 2px transparent |
| Hover | `#151515` | `#d0d0d0` | `#444444` | 2px transparent |
| Selected | `#151515` | `#d0d0d0` | `#777777` | 2px `#4fc3f7` |
| Selected + Hover | `#1c1c1c` | `#d0d0d0` | `#777777` | 2px `#4fc3f7` |

### Track row (QTableView row, via delegate)

| State | Background |
|---|---|
| Idle | `#080808` |
| Hover | `#0d0d0d` |
| Selected | `#0d3b4f` |
| Selected + Hover | `#0f4060` |
| Expanded (detail visible) | `#0e0e0e` |

Expanded state is tracked via the model (UserRole boolean), not the selection. A track can be expanded without being selected.

### Genre tag button

| State | Background | Text | Border |
|---|---|---|---|
| Idle | transparent | `#444444` | 1px `#1e1e1e` |
| Hover | `#1c1c1c` | `#d0d0d0` | 1px `#444444` |
| Pressed | `#1c1c1c` | `#d0d0d0` | 1px `#444444` |
| Active (filter applied) | `#1c1c1c` | `#d0d0d0` | 1px `#444444` |
| Active + Hover | `#222222` | `#d0d0d0` | 1px `#666666` |
| Disabled | transparent | `#2a2a2a` | 1px `#181818` |

### Folder node (clickable card in Downloads view)

| State | Background | Border |
|---|---|---|
| Idle | `#151515` | 1px `#1e1e1e` |
| Hover | `#1c1c1c` | 1px `#444444` |
| Pressed | `#151515` | 1px `#555555` |

The folder node does not have a disabled state — it is always interactive (clicking it when no path is set opens the file dialog; clicking when a path is set also opens the dialog to change it).

### Standard action button

| State | Background | Text | Border |
|---|---|---|---|
| Idle | `#151515` | `#777777` | 1px `#1e1e1e` |
| Hover | `#1c1c1c` | `#ffffff` | 1px `#444444` |
| Pressed | `#151515` | `#d0d0d0` | 1px `#555555` |
| Disabled | `#0e0e0e` | `#333333` | 1px `#1a1a1a` |
| Busy (in-progress) | `#151515` | `#4fc3f7` | 1px `#0d3b4f` — non-interactive |

### Accent button (convert all)

| State | Background | Text | Border |
|---|---|---|---|
| Idle | `#151515` | `#4fc3f7` | 1px `#0d3b4f` |
| Hover | `#0d3b4f` | `#4fc3f7` | 1px `#1a5c7a` |
| Pressed | `#0a2e3d` | `#4fc3f7` | 1px `#0d3b4f` |
| Disabled | `#0a1e28` | `#2a6a8a` | 1px `#0a2030` |

### Danger button (delete)

| State | Background | Text | Border |
|---|---|---|---|
| Idle | `#151515` | `#e57373` | 1px `#3b1515` |
| Hover | `#3b1515` | `#e57373` | 1px `#5a2020` |
| Pressed | `#2d1010` | `#e57373` | 1px `#3b1515` |
| Disabled | `#1a0a0a` | `#5a2a2a` | 1px `#220e0e` |

### Format badge (in delegate, read-only display)

Format badges are not interactive in the track list. They display only. If a future interaction is added (e.g., click to filter by format), the hover state should be: badge background lightens by approximately 15% (mix in `#ffffff` at 8% opacity), cursor changes to `Qt::PointingHandCursor`.

For the format QComboBox in the track detail panel and toolbar:

| State | Background | Border |
|---|---|---|
| Idle | `#151515` | 1px `#1e1e1e` |
| Hover | `#1c1c1c` | 1px `#444444` |
| Open | `#1c1c1c` | 1px `#444444` |
| Disabled | `#0e0e0e` | 1px `#1a1a1a` |

### Download table row action (convert/retry text button in delegate)

| State | Color |
|---|---|
| Idle | `#444444` |
| Hover (row hovered) | `#4fc3f7` |
| Pressed | `#3a9abf` |

---

## Section 9: Anti-Patterns to Avoid

### 1. `setCellWidget()` in every table row

This is the single most damaging mistake in Qt table implementation. Calling `setCellWidget()` for format badges, status indicators, or any per-row widget creates a live QWidget for every visible row. At 50 rows this is invisible. At 500 rows the UI stutters on scroll. At 5,000 rows it is unusable.

Everything that appears inside a table cell must be painted by a `QStyledItemDelegate`. The delegate's `paint()` method is called only for visible rows and is synchronous with the scroll frame. No heap allocation per row. No event handling overhead. This is non-negotiable.

The only exception: the detail panel below a selected track — this is a full-height QWidget inside a QSplitter, not a cell widget.

### 2. `Qt::TextSelectableByMouse` on display labels

Qt's default text interaction on QLabel is `Qt::LinksAccessibleByMouse | Qt::LinksAccessibleByKeyboard`. But a common mistake when debugging is to set `Qt::TextSelectableByMouse` to read label content — and then forget to remove it. In production, a user who clicks and drags across the window will select text across multiple labels, dragging a blue selection highlight through the UI exactly like a webpage.

Set `setTextInteractionFlags(Qt::NoTextInteraction)` on every display-only QLabel explicitly. There should be no selectable text in this application except inside the search QLineEdit and the activity log QPlainTextEdit.

### 3. Hardcoded colors in C++ paint code bypassing the palette

If a QWidget subclass or delegate references `QColor(0xd0, 0xd0, 0xd0)` directly in `paintEvent()`, it will ignore QPalette and QSS overrides. More critically, it will look wrong in Windows high-contrast accessibility mode and on any system where the user has configured custom color themes.

Color values that are structural (border `#1e1e1e`, row separator `#0f0f0f`) must be defined as `static const QColor` in a single header (`ThemeColors.h`), or read from a `QSettings`-backed theme object. They must not be scattered as magic hex values across paint methods. The QSS file defines the canonical values — the delegate reads from the same source, not from a second copy.

### 4. Fixed pixel sizes for spacing, margins, and row heights

Setting `setFixedHeight(30)` for a button, `setContentsMargins(10, 10, 10, 10)` for a panel, or `verticalHeader()->setDefaultSectionSize(30)` for rows produces an application that looks compressed on 4K displays and bloated on 1080p displays with 125% system scaling.

All spacing that is not a row height (which has design-specific reasoning at 54px) should be derived from `QFontMetrics::height()` or `QApplication::font()` metrics. For example, a comfortable padding around a label is `fontMetrics().height() * 0.6`. This scales with the system font size and DPI simultaneously.

The exception: fixed structural widths for the sidebar (220px) and playlist panel (320px) are intentional design constraints, not arbitrary values. They are set in logical pixels and Qt's high-DPI system handles physical scaling automatically.

### 5. Subclassing QThread and putting business logic in `run()`

The conversion worker, folder watcher, and any database operation that could block must run on a dedicated QThread. The correct pattern is the `moveToThread()` worker object pattern documented in `QT_MIGRATION.md`. The anti-pattern is:

```cpp
// DO NOT DO THIS
class ConverterThread : public QThread {
    void run() override {
        // business logic here — cannot safely access UI
        ffmpeg.waitForFinished(-1);
        emit done(); // cross-thread signal — OK if connected with Qt::QueuedConnection
        // but the above pattern is an anti-pattern regardless
    }
};
```

The reason this matters beyond architecture purity: logic in `run()` is difficult to test, cannot receive queued signals correctly, and leads to subtle race conditions when the thread needs to be shut down cleanly. The worker object pattern gives clean `start()` / `stop()` control via slots and integrates naturally with `QUndoStack`.

### 6. Using QTableWidget instead of QAbstractTableModel + QTableView

`QTableWidget` is `QTableView` with a built-in `QStandardItemModel`. It is convenient for 50 rows. A Rekordbox library can have 10,000+ tracks. `QStandardItemModel` stores every cell as a `QStandardItem` heap allocation. `QAbstractTableModel` with a `QVector<Track>` backing store uses zero per-cell allocation — all data is returned on demand via `data()`. The difference at scale is not a minor optimization — it is the difference between a usable and an unusable application.

Additionally, `QTableWidget` cannot use `canFetchMore()` / `fetchMore()` for lazy database loading. A `QAbstractTableModel` subclass can batch-load rows in groups of 200 as the user scrolls, keeping the initial load time under 50ms regardless of library size.

### 7. Using the system default QStyle for selection highlight

The default system selection color on Windows is the Windows accent color (usually blue). On macOS it is the system accent (blue or graphite). Neither of these is `#0d3b4f`. If the QPalette override described in Section 3 is not applied, selected rows will flash the OS accent color, breaking the entire visual language of the application.

Override it. Both the QSS `selection-background-color` and the `QPalette::Highlight` / `QPalette::HighlightedText` colors must be set explicitly. QSS alone is not sufficient because QPainter-based selection highlight in delegates may read directly from the palette, bypassing the stylesheet.

### 8. Global `setStyleSheet()` on QApplication

`QApplication::setStyleSheet(fullSheet)` applies the stylesheet globally to every widget, including internal Qt widgets (popup menus within QComboBox, QScrollBar inside QComboBox's popup, etc.). The stylesheet in this project is designed to be loaded at the QMainWindow level:

```cpp
mainWindow->setStyleSheet(loadedQssString);
```

This scopes the rules to the main window and its children, which is the entire visible application. If QDialog or QMenu inherits from the window, they will pick up the styles correctly. If you load a global stylesheet and then open a QColorDialog or QFontDialog that Qt renders internally, those dialogs will have their native styling destroyed by your rules. Scope matters.

---

*End of design specification. This document is the authoritative visual reference for the Qt implementation. Deviation requires a written rationale and revision of this document.*
