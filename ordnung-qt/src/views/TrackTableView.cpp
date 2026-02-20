#include "views/TrackTableView.h"

#include "models/TrackModel.h"
#include "delegates/FormatDelegate.h"
#include "views/table/LibraryTableColumn.h"
#include "style/Theme.h"

#include <QAbstractItemModel>
#include <QHeaderView>
#include <QFontMetrics>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QResizeEvent>
#include <QUndoStack>
#include <QPalette>
#include <QColor>

// ── GenreFilterProxy ─────────────────────────────────────────────────────────

GenreFilterProxy::GenreFilterProxy(QObject* parent)
    : QSortFilterProxyModel(parent)
{}

void GenreFilterProxy::setGenreFilter(const QString& genre)
{
    m_genreFilter = genre;
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
    invalidateFilter();
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif
}

void GenreFilterProxy::setSearchText(const QString& text)
{
    m_searchText = text.trimmed();
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
    invalidateFilter();
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif
}

bool GenreFilterProxy::filterAcceptsRow(int sourceRow, const QModelIndex& /*sourceParent*/) const
{
    auto* tm = qobject_cast<TrackModel*>(sourceModel());
    if (!tm || sourceRow >= tm->tracks().size())
        return true;

    const Track& t = tm->tracks()[sourceRow];

    if (!m_searchText.isEmpty()) {
        const QString title  = QString::fromStdString(t.title);
        const QString artist = QString::fromStdString(t.artist);
        const QString album  = QString::fromStdString(t.album);
        const QString genre  = QString::fromStdString(t.genre);
        const QString keySig = QString::fromStdString(t.key_sig);

        if (!title.contains(m_searchText,  Qt::CaseInsensitive)
         && !artist.contains(m_searchText, Qt::CaseInsensitive)
         && !album.contains(m_searchText,  Qt::CaseInsensitive)
         && !genre.contains(m_searchText,  Qt::CaseInsensitive)
         && !keySig.contains(m_searchText, Qt::CaseInsensitive))
            return false;
    }

    if (!m_genreFilter.isEmpty()) {
        const QString genre = QString::fromStdString(t.genre);
        if (!genre.contains(m_genreFilter, Qt::CaseInsensitive))
            return false;
    }

    return true;
}

// ── TrackTableView ───────────────────────────────────────────────────────────

TrackTableView::TrackTableView(TrackModel* model, QUndoStack* undoStack, QWidget* parent)
    : QTableView(parent)
    , m_trackModel(model)
    , m_proxy(new GenreFilterProxy(this))
    , m_delegate(new FormatDelegate(this))
    , m_undoStack(undoStack)
{
    m_proxy->setSourceModel(model);
    setModel(m_proxy);
    setItemDelegate(m_delegate);

    connect(m_proxy, &QAbstractItemModel::modelAboutToBeReset,
            this, [this]() { m_expandedTrackId = -1; });

    // Scroll
    setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);

    // Selection
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    setSelectionBehavior(QAbstractItemView::SelectRows);

    // Editing — double-click to edit, Enter to confirm
    setEditTriggers(QAbstractItemView::DoubleClicked);

    // Visual
    setShowGrid(false);
    setAlternatingRowColors(false);
    setMouseTracking(true);
    viewport()->setMouseTracking(true);
    verticalHeader()->hide();
    verticalHeader()->setDefaultSectionSize(Theme::Layout::TrackRowH);
    setSortingEnabled(true);

    // User can resize columns (Interactive); we distribute width by m_columnWeights on window resize.
    const int colCount = LibraryTableColumn::columnCount();
    m_columnWeights.resize(colCount);
    m_columnMinWidths.resize(colCount);
    {
        const QFontMetrics fm(horizontalHeader()->font());
        const int pad = 12;
        for (int c = 0; c < colCount; ++c) {
            m_columnWeights[c] = LibraryTableColumn::columnProps(c).fixedWidth;
            const int textW = fm.horizontalAdvance(LibraryTableColumn::headerText(c));
            m_columnMinWidths[c] = textW + pad;
        }
    }

    auto* hdr = horizontalHeader();
    const int lastCol = colCount - 1;
    for (int c = 0; c < colCount; ++c) {
        const bool isFixedWidthCol = (c == LibraryTableColumn::columnIndex(LibraryTableColumn::Bpm)
                                      || c == LibraryTableColumn::columnIndex(LibraryTableColumn::Key)
                                      || c == LibraryTableColumn::columnIndex(LibraryTableColumn::Time));
        const bool isLastCol = (c == lastCol);
        // Last column uses Stretch so it always takes the remaining space and never spills out.
        if (isLastCol)
            hdr->setSectionResizeMode(c, QHeaderView::Stretch);
        else
            hdr->setSectionResizeMode(c, isFixedWidthCol ? QHeaderView::Fixed : QHeaderView::Interactive);
        hdr->setMinimumSectionSize(m_columnMinWidths[c]);
    }
    hdr->setFixedHeight(Theme::Layout::HeaderH);
    connect(hdr, &QHeaderView::sectionResized, this, &TrackTableView::onSectionResized);

    m_columnWidths.resize(colCount);
    for (int c = 0; c < colCount; ++c)
        m_columnWidths[c] = LibraryTableColumn::columnProps(c).fixedWidth;
    applyColumnVisibilityAndWidths();

    // Override system selection color
    QPalette p = palette();
    p.setColor(QPalette::Highlight,       QColor(Theme::Color::AccentBg));
    p.setColor(QPalette::HighlightedText, QColor(Theme::Color::Text));
    setPalette(p);
}

void TrackTableView::setSearchText(const QString& text)
{
    m_proxy->setSearchText(text);
}

void TrackTableView::setGenreFilter(const QString& genre)
{
    m_proxy->setGenreFilter(genre);
}

QVariantMap TrackTableView::toggleRow(int proxyRow)
{
    const QModelIndex proxyIdx  = m_proxy->index(proxyRow, 0);
    const QModelIndex sourceIdx = m_proxy->mapToSource(proxyIdx);
    if (!sourceIdx.isValid())
        return {};

    const int  srcRow      = sourceIdx.row();
    const bool wasExpanded = m_trackModel->data(sourceIdx, TrackModel::ExpandedRole).toBool();
    const bool nowExpanded = !wasExpanded;
    const long long trackId = m_trackModel->data(sourceIdx, TrackModel::TrackIdRole).toLongLong();

    // Collapse any previously expanded row, identified by ID (proxy rows shuffle after sort).
    if (m_expandedTrackId != -1 && m_expandedTrackId != trackId) {
        const int prevSrcRow = m_trackModel->rowForId(m_expandedTrackId);
        if (prevSrcRow >= 0)
            m_trackModel->setExpanded(prevSrcRow, false);
    }

    m_trackModel->setExpanded(srcRow, nowExpanded);
    m_expandedTrackId = nowExpanded ? trackId : -1LL;

    if (nowExpanded) {
        const Track& t = m_trackModel->tracks()[srcRow];
        QVariantMap map;
        map["id"]       = (long long)t.id;
        map["title"]    = QString::fromStdString(t.title);
        map["artist"]   = QString::fromStdString(t.artist);
        map["album"]    = QString::fromStdString(t.album);
        map["genre"]    = QString::fromStdString(t.genre);
        map["bpm"]      = t.bpm;
        map["rating"]   = t.rating;
        map["time"]     = QString::fromStdString(t.time);
        map["key"]      = QString::fromStdString(t.key_sig);
        map["added"]    = QString::fromStdString(t.date_added);
        map["format"]   = QString::fromStdString(t.format);
        map["has_aiff"] = t.has_aiff;
        map["filepath"] = QString::fromStdString(t.filepath);
        emit trackExpanded(map);
        return map;
    }

    emit trackCollapsed();
    return {};
}

QVector<QPair<int, long long>> TrackTableView::visibleTrackIds() const
{
    QVector<QPair<int, long long>> result;
    const int rows = m_proxy->rowCount();
    result.reserve(rows);
    for (int i = 0; i < rows; ++i) {
        const QModelIndex proxyIdx  = m_proxy->index(i, 0);
        const QModelIndex sourceIdx = m_proxy->mapToSource(proxyIdx);
        const long long id = m_trackModel->data(sourceIdx, TrackModel::TrackIdRole).toLongLong();
        result.append({sourceIdx.row(), id});
    }
    return result;
}

QRect TrackTableView::thumbnailRect(const QRect& cellRect)
{
    const int sz = Theme::Layout::TrackThumbSize;
    const int pad = Theme::Layout::TrackThumbPad;
    const int y = cellRect.top() + (cellRect.height() - sz) / 2;
    return QRect(cellRect.left() + pad, y, sz, sz);
}

void TrackTableView::tryPlayFromThumbnail(const QModelIndex& proxyIdx, const QPoint& pos)
{
    if (proxyIdx.column() != LibraryTableColumn::columnIndex(LibraryTableColumn::Title))
        return;
    const QRect cellRect = visualRect(proxyIdx);
    if (!thumbnailRect(cellRect).contains(pos))
        return;
    const QModelIndex sourceIdx = m_proxy->mapToSource(proxyIdx);
    if (!sourceIdx.isValid() || sourceIdx.row() >= m_trackModel->tracks().size())
        return;
    const Track& t = m_trackModel->tracks()[sourceIdx.row()];
    if (t.filepath.empty())
        return;
    emit playRequested(QString::fromStdString(t.filepath),
                      QString::fromStdString(t.title),
                      QString::fromStdString(t.artist));
}

void TrackTableView::mousePressEvent(QMouseEvent* event)
{
    const QModelIndex proxyIdx = indexAt(event->pos());
    if (proxyIdx.isValid()) {
        const int colorCol = LibraryTableColumn::columnIndex(LibraryTableColumn::Color);
        const bool inThumb = proxyIdx.column() == LibraryTableColumn::columnIndex(LibraryTableColumn::Title)
                             && thumbnailRect(visualRect(proxyIdx)).contains(event->pos());
        if (proxyIdx.column() == colorCol) {
            // Click on Color column cycles the Pioneer label 0→1→…→8→0.
            // setColorLabel also persists to DB.
            const QModelIndex srcIdx = m_proxy->mapToSource(proxyIdx);
            if (srcIdx.isValid() && srcIdx.row() < m_trackModel->tracks().size()) {
                const int cur = m_trackModel->tracks()[srcIdx.row()].color_label;
                m_trackModel->setColorLabel(srcIdx.row(), (cur + 1) % 9);
            }
        } else if (inThumb) {
            tryPlayFromThumbnail(proxyIdx, event->pos());
        } else {
            toggleRow(proxyIdx.row());
        }
    }
    QTableView::mousePressEvent(event);
}

void TrackTableView::mouseDoubleClickEvent(QMouseEvent* event)
{
    // Double-click starts cell editing (Enter to confirm). Let base handle it.
    QTableView::mouseDoubleClickEvent(event);
}

void TrackTableView::keyPressEvent(QKeyEvent* event)
{
    // When a cell is being edited, Return commits the edit — don't intercept it.
    if (state() != QAbstractItemView::EditingState
            && (event->key() == Qt::Key_Return || event->key() == Qt::Key_Space)
            && currentIndex().isValid()) {
        toggleRow(currentIndex().row());
        return;
    }
    QTableView::keyPressEvent(event);
}

void TrackTableView::mouseMoveEvent(QMouseEvent* event)
{
    const QModelIndex proxyIdx = indexAt(event->pos());
    const int row = proxyIdx.isValid() ? proxyIdx.row() : -1;
    const bool overThumb = proxyIdx.isValid() && proxyIdx.column() == LibraryTableColumn::columnIndex(LibraryTableColumn::Title)
                          && thumbnailRect(visualRect(proxyIdx)).contains(event->pos());

    if (row != m_hoveredRow) {
        m_hoveredRow = row;
        viewport()->update();
    }
    if (overThumb != m_hoveredThumb) {
        m_hoveredThumb = overThumb;
        setCursor(overThumb ? Qt::PointingHandCursor : Qt::ArrowCursor);
    }
    QTableView::mouseMoveEvent(event);
}

void TrackTableView::onSectionResized(int /*logicalIndex*/, int /*oldSize*/, int /*newSize*/)
{
    // Store current column widths so window resize only adjusts the last column (Rekordbox-style: only the dragged boundary moves).
    const int colCount = LibraryTableColumn::columnCount();
    if (m_columnWidths.size() == colCount) {
        for (int c = 0; c < colCount; ++c)
            m_columnWidths[c] = horizontalHeader()->sectionSize(c);
    }
}

void TrackTableView::applyColumnVisibilityAndWidths()
{
    const int w = viewport()->width();
    if (w <= 0)
        return;

    const int colCount = LibraryTableColumn::columnCount();

    int criticalMinTotal = 0;
    int criticalCount = 0;
    for (int c = 0; c < colCount; ++c) {
        const auto props = LibraryTableColumn::columnProps(c);
        if (props.critical) {
            criticalMinTotal += props.fixedWidth;
            ++criticalCount;
        }
    }
    if (criticalMinTotal <= 0)
        criticalMinTotal = 400;

    // Narrow window: show only critical columns, distribute width by weights.
    if (w < criticalMinTotal && criticalCount > 0) {
        int criticalWeightTotal = 0;
        for (int c = 0; c < colCount; ++c)
            if (LibraryTableColumn::columnProps(c).critical)
                criticalWeightTotal += m_columnWeights[c];
        if (criticalWeightTotal <= 0)
            criticalWeightTotal = 1;
        int assigned = 0;
        int criticalIdx = 0;
        for (int c = 0; c < colCount; ++c) {
            const auto props = LibraryTableColumn::columnProps(c);
            horizontalHeader()->setSectionHidden(c, !props.critical);
            if (props.critical) {
                const int minW = c < m_columnMinWidths.size() ? m_columnMinWidths[c] : 40;
                int colW;
                if (criticalCount == 1)
                    colW = w;
                else if (criticalIdx == criticalCount - 1)
                    colW = w - assigned;
                else
                    colW = qMax(minW, (w * m_columnWeights[c]) / criticalWeightTotal);
                setColumnWidth(c, colW);
                assigned += colW;
                ++criticalIdx;
            }
        }
        return;
    }

    // Wide enough: show all columns. BPM, Key, Time always fixed; Title, Artist use stored width; Format (last) gets the remainder (Rekordbox-style).
    for (int c = 0; c < colCount; ++c)
        horizontalHeader()->setSectionHidden(c, false);

    if (m_columnWidths.size() != colCount)
        return;

    const int bpmCol  = LibraryTableColumn::columnIndex(LibraryTableColumn::Bpm);
    const int keyCol  = LibraryTableColumn::columnIndex(LibraryTableColumn::Key);
    const int timeCol = LibraryTableColumn::columnIndex(LibraryTableColumn::Time);

    for (int c = 0; c < colCount - 1; ++c) {
        int width;
        if (c == bpmCol || c == keyCol || c == timeCol)
            width = LibraryTableColumn::columnProps(c).fixedWidth;
        else
            width = qMax(m_columnMinWidths.value(c, 24), m_columnWidths[c]);
        setColumnWidth(c, width);
    }
    // Last column has Stretch resize mode; it gets the remainder automatically and stays within the viewport.
}

void TrackTableView::resizeEvent(QResizeEvent* event)
{
    QTableView::resizeEvent(event);
    const int w = viewport()->width();
    if (w <= 0) return;

    const int colCount = LibraryTableColumn::columnCount();
    int criticalMinTotal = 0;
    int criticalCount = 0;
    for (int c = 0; c < colCount; ++c) {
        const auto props = LibraryTableColumn::columnProps(c);
        if (props.critical) {
            criticalMinTotal += props.fixedWidth;
            ++criticalCount;
        }
    }
    if (criticalMinTotal <= 0) criticalMinTotal = 400;

    if (w < criticalMinTotal && criticalCount > 0) {
        applyColumnVisibilityAndWidths();
        if (m_columnWidths.size() == colCount)
            for (int c = 0; c < colCount; ++c)
                m_columnWidths[c] = horizontalHeader()->sectionSize(c);
    } else {
        applyColumnVisibilityAndWidths();
    }
}

void TrackTableView::leaveEvent(QEvent* event)
{
    m_hoveredRow = -1;
    if (m_hoveredThumb) {
        m_hoveredThumb = false;
        unsetCursor();
    }
    viewport()->update();
    QTableView::leaveEvent(event);
}
