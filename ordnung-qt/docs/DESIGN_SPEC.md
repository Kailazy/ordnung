# DESIGN SPEC — ExportWizard + AnalysisProgressDialog

**Project:** Eyebags Terminal (Qt C++ desktop DJ library manager)
**Design author:** dj-ui-architect
**Implementer:** frontend-architect
**Date:** 2026-02-20
**Theme file:** `resources/theme.qss`

---

## Design language ground rules

Everything in this spec inherits from the established aesthetic documented in `theme.qss`. Do not deviate. The relevant axioms:

- Dark charcoal surfaces. No white, no light grey, no gradients.
- Sharp corners. `border-radius: 0–2px` maximum on all interactive elements.
- Accent orange `#ff764d` is the *only* interactive color. Use it sparingly: selected state, focus ring, primary action.
- Monospace (`Consolas`) for all data labels, step indicators, stats, paths. Proportional (`Figtree`) for human-readable text.
- 8px grid. All padding/margin values are multiples of 4px.
- No shadows, no elevation layers. Depth is communicated through background offset alone (`#1a1a1a` → `#222222` → `#2a2a2a` → `#353535`).

---

## Component 1: ExportWizard

### Intent

A guided two-step flow. Step 1 asks *where* to export; step 2 asks *what* and *how*. The user must commit each step before advancing. The wizard should feel like a hardware menu — deliberate, sequential, no ambiguity.

### Container

- **Widget type:** `QDialog` subclass named `ExportWizard`
- **Object name:** `exportWizard`
- **Size:** Fixed `640 × 520px`. Use `setFixedSize(640, 520)`.
- **Modality:** `Qt::ApplicationModal`. Centre on parent window using `QDialog::exec()`.
- **Root layout:** `QVBoxLayout`, zero margins, zero spacing. Three sections stacked:
  1. Header area (step indicator + section title)
  2. Body area (step-specific content, fills remaining space)
  3. Footer area (Back / Next·Export buttons + progress strip)

### Widget tree — full hierarchy

```
QDialog#exportWizard  [640×520, fixed]
└── QVBoxLayout (margins: 0, spacing: 0)
    ├── QWidget#exportHeader  [height: 64px]
    │   └── QVBoxLayout (margins: 24, 16, 24, 12)
    │       ├── QLabel#exportStep         "01  TARGET"
    │       └── QLabel#exportSectionTitle "Select export format"
    │
    ├── QStackedWidget  [stretch: 1]
    │   ├── [Page 0] Step 1 widget  (see Step 1 below)
    │   └── [Page 1] Step 2 widget  (see Step 2 below)
    │
    ├── QWidget#exportFooter  [height: 56px]
    │   └── QHBoxLayout (margins: 24, 12, 24, 12)
    │       ├── QPushButton#exportBackBtn  "BACK"
    │       ├── QSpacerItem (expanding)
    │       └── QPushButton#exportConfirmBtn  "NEXT" / "EXPORT"
    │
    └── QProgressBar#exportProgressStrip  [height: 4px, hidden until export runs]
```

### Header area

- `QWidget#exportHeader`: `background-color: #1a1a1a`, `border-bottom: 1px solid #2d2d2d`
- `QLabel#exportStep`: Consolas 10pt, letter-spacing 3px, color `#5a5a5a`. Text cycles:
  - Step 1: `"01  TARGET"`
  - Step 2: `"02  OPTIONS"`
- `QLabel#exportSectionTitle`: Figtree 14pt, color `#d8d8d8`. Text cycles:
  - Step 1: `"Select export format"`
  - Step 2: `"Configure export"`

### Step 1 — Target selection

**Page 0 of the QStackedWidget.**

```
QWidget  [full page, margins: 24px all]
└── QVBoxLayout (spacing: 16)
    ├── QHBoxLayout (spacing: 16)  ← card row
    │   ├── QPushButton#exportTypeCard  "REKORDBOX XML"
    │   └── QPushButton#exportTypeCard  "CDJ USB EXPORT"
    └── QHBoxLayout (spacing: 8)   ← path row
        ├── QLineEdit#exportPathInput   (stretch: 1)
        └── QPushButton#exportBrowseBtn "BROWSE"
```

**Card design — QPushButton#exportTypeCard:**

Each card is a `QPushButton` with `setCheckable(false)`. Mutual exclusion is managed in code (not a QButtonGroup, because we need the custom `selected` property for QSS). When the user clicks a card, the code calls:
```cpp
cardA->setProperty("selected", cardA == clicked);
cardB->setProperty("selected", cardB == clicked);
cardA->style()->unpolish(cardA); cardA->style()->polish(cardA);
cardB->style()->unpolish(cardB); cardB->style()->polish(cardB);
```

Each card contains a `QVBoxLayout` (margins: 24px horizontal, 20px vertical, spacing: 8) with:
- `QLabel#exportCardTitle` — Consolas 14pt bold, uppercase format name: `"REKORDBOX XML"` or `"CDJ USB EXPORT"`
- `QLabel#exportCardDesc` — Figtree 12pt, muted, single line:
  - Rekordbox: `"Export to .xml for Rekordbox import"`
  - CDJ USB: `"Copy tracks to USB with Engine-compatible structure"`

Cards sit in an `QHBoxLayout`. Both cards take equal width (`setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed)`) with `min-height: 140px`.

**Path row:**

- `QLabel` above the row (not named, use default styling): Figtree 12pt, color `#5a5a5a`. Text:
  - Rekordbox selected: `"XML output path"`
  - CDJ USB selected: `"USB mount point"`
  - Update this label text when card selection changes.
- `QLineEdit#exportPathInput`: stretches to fill. Placeholder text matches the label above.
- `QPushButton#exportBrowseBtn`: fixed width ~80px, opens `QFileDialog::getSaveFileName` (XML) or `QFileDialog::getExistingDirectory` (USB) depending on selected card.

### Step 2 — Options

**Page 1 of the QStackedWidget.**

```
QWidget  [full page, margins: 24px horizontal, 16px vertical]
└── QHBoxLayout (spacing: 32)
    ├── QVBoxLayout (stretch: 1)  ← left column: playlist checklist
    │   ├── QLabel#exportColHeader  "PLAYLISTS"
    │   ├── QScrollArea  (stretch: 1)
    │   │   └── QWidget#exportPlaylistScroll
    │   │       └── QVBoxLayout (spacing: 4, margins: 8)
    │   │           └── [QCheckBox per playlist — name as label, no object name needed]
    │   └── (bottom spacer)
    │
    └── QVBoxLayout  ← right column: output options
        ├── QLabel#exportColHeader  "OUTPUT FORMAT"
        ├── QRadioButton  "Keep original"
        ├── QRadioButton  "Convert to AIFF"
        ├── QRadioButton  "Convert to WAV"
        ├── QSpacerItem  (16px fixed)
        ├── QLabel#exportColHeader  "FILE HANDLING"
        ├── QCheckBox  "Copy files to destination"
        ├── QSpacerItem  (expanding)
        ├── QLabel#exportColHeader  "ESTIMATED SIZE"
        └── QLabel#exportSizeEstimate  "— GB"
```

**Left column — Playlist checklist:**

- `QLabel#exportColHeader`: Consolas 10pt, letter-spacing 2px, `#5a5a5a`. All column section headers use this object name.
- `QScrollArea`: `setFrameShape(QFrame::NoFrame)`, `setWidgetResizable(true)`, `setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff)`.
- Inner `QWidget#exportPlaylistScroll`: `background-color: #222222`, `border: 1px solid #2d2d2d`.
- Each `QCheckBox` has no custom object name — inherits global `QCheckBox` QSS rules. Label text = playlist name. `setChecked(true)` by default (export all, user deselects to exclude).

**Right column — Output options:**

- Column has fixed width of ~220px. Use `setFixedWidth(220)` on the column wrapper.
- `QRadioButton` rows have default QSS inherited from global radio button rules added to theme.qss. All three are in a `QButtonGroup` to enforce mutual exclusion. "Keep original" is checked by default.
- "Copy files" checkbox: checked by default for CDJ USB mode, unchecked for Rekordbox XML mode. Update default when target card selection changes.
- `QLabel#exportSizeEstimate`: updated whenever playlist selection changes. Format: `"≈ 4.2 GB"`. If nothing selected: `"—"`. Calculation can be approximate (sum of file sizes from DB).

### Footer

- `QWidget#exportFooter`: 56px fixed height, `background-color: #1a1a1a`, `border-top: 1px solid #2d2d2d`.
- `QPushButton#exportBackBtn "BACK"`: on Step 1, this button is `setEnabled(false)` (not hidden — its disabled state communicates step position).
- `QPushButton#exportConfirmBtn`: text is `"NEXT"` on Step 1, `"EXPORT"` on Step 2. On Step 1 click: advance QStackedWidget, update header labels, swap button text. On Step 2 click: begin export, show progress strip, disable both buttons.

### Progress strip

- `QProgressBar#exportProgressStrip`: 4px fixed height. `setTextVisible(false)`. `setRange(0, 100)`.
- Hidden (`setVisible(false)`) when dialog opens. Made visible when export begins.
- Sits as the last widget in the root `QVBoxLayout`, outside the footer widget.
- On completion: set to 100, brief 400ms pause, then `accept()` the dialog.

---

## Component 2: AnalysisProgressDialog

### Intent

A non-blocking progress indicator for background library analysis (BPM detection, key analysis, etc.). Compact, always-on-top, non-resizable. The user can cancel but not interact with the main window while it is open. It should read as a transient system process — not a wizard, not a form. A machine doing a task.

### Container

- **Widget type:** `QDialog` subclass named `AnalysisProgressDialog`
- **Object name:** `analysisDialog`
- **Size:** Fixed `480 × 200px`. Use `setFixedSize(480, 200)`.
- **Modality:** `Qt::ApplicationModal`.
- **Window flags:** `Qt::Dialog | Qt::FramelessWindowHint` — use a custom 1px border drawn by the QDialog's own stylesheet. The QSS rule `QDialog#analysisDialog { border: 1px solid #2d2d2d; }` handles this.
- **Root layout:** `QVBoxLayout`, zero margins, zero spacing. Three rows:
  1. Title row (header)
  2. Body (filename + progress bar + stats)
  3. Footer (cancel button)

### Widget tree — full hierarchy

```
QDialog#analysisDialog  [480×200, fixed, frameless]
└── QVBoxLayout (margins: 0, spacing: 0)
    ├── QWidget#analysisHeader  [height: 48px]
    │   └── QHBoxLayout (margins: 24, 14, 24, 14, spacing: 8)
    │       ├── QLabel#analysisTitle  "ANALYZING LIBRARY"
    │       ├── QLabel#analysisDot    (6×6px pulsing square)
    │       └── QSpacerItem (expanding)
    │
    ├── QWidget  [body, stretch: 1]  (no object name, transparent)
    │   └── QVBoxLayout (margins: 24, 12, 24, 16, spacing: 10)
    │       ├── QLabel#analysisFilename  (elided current filename)
    │       ├── QProgressBar#analysisProgress  [6px height]
    │       └── QHBoxLayout (spacing: 0)
    │           ├── QLabel#analysisStat  "312 / 1048"
    │           ├── QSpacerItem (expanding)
    │           └── QLabel#analysisStat  "0:48"
    │
    └── QWidget#analysisFooter  [height: 52px]
        └── QHBoxLayout (margins: 24, 12, 24, 12, spacing: 0)
            ├── QSpacerItem (expanding)
            └── QPushButton#analysisCancelBtn  "CANCEL"
```

### Header row

- `QWidget#analysisHeader`: `background-color: #1a1a1a`, `border-bottom: 1px solid #2d2d2d`.
- `QLabel#analysisTitle`: Consolas 11pt, letter-spacing 3px, `#5a5a5a`, uppercase. Static text: `"ANALYZING LIBRARY"`.
- `QLabel#analysisDot`: The 6×6px pulsing square. Same visual contract as the existing `logDot` in the activity log header. In C++, drive with a `QTimer` (800ms interval) that toggles the dot's background color between `#ff764d` (active) and `#5a5a5a` (idle) via `setStyleSheet()`. Note: a QPropertyAnimation on `background-color` does not work in Qt QSS — use a timer.

### Body

- `QLabel#analysisFilename`: display the filename stem only (strip directory path in code). Elide right via `setElideMode` on a custom label subclass, or use a `QFontMetrics::elidedText()` call in `setText()`. Figtree 13pt, primary text color `#d8d8d8`.
- `QProgressBar#analysisProgress`: `setTextVisible(false)`. `setRange(0, totalTrackCount)`. `setValue(currentIndex)` on each analysis completion signal.
- Count stat label: `"312 / 1048"` format. Update on each track completion.
- Elapsed time stat label: `"0:48"` format (mm:ss). Drive from a `QTimer` started when analysis begins, update every second.

### Footer

- `QWidget#analysisFooter`: matches `exportFooter` — `#1a1a1a` bg, 1px top border `#2d2d2d`.
- `QPushButton#analysisCancelBtn`: right-aligned. On click: emit `cancelRequested()` signal, set button to `setEnabled(false)`, change label text to `"CANCELING..."`. The owning analysis service responds to the signal and calls `accept()` or `reject()` on the dialog when the current track finishes.

---

## Object name registry

All object names used in QSS rules for these two components:

| Object name | Widget type | Component | Notes |
|---|---|---|---|
| `exportWizard` | `QDialog` | ExportWizard | Root dialog |
| `exportStep` | `QLabel` | ExportWizard | "01  TARGET" / "02  OPTIONS" |
| `exportSectionTitle` | `QLabel` | ExportWizard | Step title |
| `exportTypeCard` | `QPushButton` | ExportWizard | Both target cards share this name; `selected` property drives QSS |
| `exportCardTitle` | `QLabel` | ExportWizard | Inside card layout |
| `exportCardDesc` | `QLabel` | ExportWizard | Inside card layout |
| `exportPathInput` | `QLineEdit` | ExportWizard | Output path / mount point |
| `exportBrowseBtn` | `QPushButton` | ExportWizard | Opens file/folder dialog |
| `exportColHeader` | `QLabel` | ExportWizard | Reused for all column section titles |
| `exportPlaylistScroll` | `QWidget` | ExportWizard | Inner widget of playlist QScrollArea |
| `exportSizeEstimate` | `QLabel` | ExportWizard | "≈ 4.2 GB" |
| `exportFooter` | `QWidget` | ExportWizard | Footer bar |
| `exportBackBtn` | `QPushButton` | ExportWizard | Back / disabled on step 1 |
| `exportConfirmBtn` | `QPushButton` | ExportWizard | "NEXT" / "EXPORT" |
| `exportProgressStrip` | `QProgressBar` | ExportWizard | 4px strip, hidden until export |
| `analysisDialog` | `QDialog` | AnalysisProgressDialog | Root dialog |
| `analysisTitle` | `QLabel` | AnalysisProgressDialog | "ANALYZING LIBRARY" |
| `analysisDot` | `QLabel` | AnalysisProgressDialog | Pulsing 6×6 square |
| `analysisFilename` | `QLabel` | AnalysisProgressDialog | Current file, elided |
| `analysisProgress` | `QProgressBar` | AnalysisProgressDialog | 6px height, no text |
| `analysisStat` | `QLabel` | AnalysisProgressDialog | Reused for count and elapsed time |
| `analysisFooter` | `QWidget` | AnalysisProgressDialog | Footer bar |
| `analysisCancelBtn` | `QPushButton` | AnalysisProgressDialog | Danger style |

---

## State transitions

### ExportWizard

| State | Visible / Enabled elements |
|---|---|
| Step 1, no card selected | exportConfirmBtn disabled, exportPathInput disabled, exportBrowseBtn disabled |
| Step 1, card selected | All enabled. Path input placeholder text reflects selected target. |
| Step 2 | exportBackBtn enabled. exportConfirmBtn text = "EXPORT". |
| Export running | Both buttons disabled. exportProgressStrip visible, value updating. |
| Export complete | Dialog closes via `accept()`. Caller shows success feedback in main window activity log. |

### AnalysisProgressDialog

| State | Description |
|---|---|
| Running | analysisDot pulsing. Progress bar advancing. Filename and stats updating. Cancel button enabled. |
| Cancel requested | analysisCancelBtn disabled, text = "CANCELING...". Analysis completes current track then stops. |
| Finished | Dialog closes via `accept()`. |

---

## Implementation notes for frontend-architect

1. **Property-based QSS:** The `selected` property on `exportTypeCard` requires calling `style()->unpolish(widget)` then `style()->polish(widget)` after `setProperty()` for Qt to re-evaluate the selector. This pattern is already established in the codebase (see `navBtn[active="true"]` in `MainWindow`).

2. **QStackedWidget transition:** No animation. Simply call `setCurrentIndex(1)` on NEXT click and `setCurrentIndex(0)` on BACK click. Update header labels and button text in the same handler.

3. **analysisDot pulsing:** Do not attempt `QPropertyAnimation` on `background-color` — Qt's QSS engine does not interpolate color properties. Use a `QTimer` with a 400ms interval toggling between two explicit `setStyleSheet()` calls. The same approach applies to any future pulsing dot widgets.

4. **Frameless dialog border:** `Qt::FramelessWindowHint` removes the native title bar. The QSS `border: 1px solid #2d2d2d` on `QDialog#analysisDialog` provides the visual frame. On some platform themes, this may require `setAttribute(Qt::WA_StyledBackground, true)` on the dialog.

5. **Fixed-size dialogs:** Both dialogs use `setFixedSize()`. Do not call `setSizeGripEnabled(true)`. `QSizePolicy` on child widgets should be set so that they do not fight the fixed outer dimension.

6. **Playlist checklist data:** The ExportWizard receives a list of playlists on construction (passed by the calling view). It does not query the database directly. The `ExportService` (separate component, task #2) receives the wizard's selected playlist IDs and options struct on `accept()`.

7. **exportProgressStrip placement:** The strip must sit *outside* the `exportFooter` widget — it is the last child of the root `QVBoxLayout`. This ensures the footer's top border and the strip's fill are visually adjacent without any gap.
