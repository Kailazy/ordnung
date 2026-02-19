#include "views/TrackTableView.h"

#include "models/TrackModel.h"
#include "delegates/FormatDelegate.h"

#include <QHeaderView>
#include <QMouseEvent>
#include <QUndoStack>
#include <QPalette>
#include <QColor>

// ── GenreFilterProxy ─────────────────────────────────────────────────────────

GenreFilterProxy::GenreFilterProxy(QObject* parent)
    : QSortFilterProxyModel(parent)
{}

void GenreFilterProxy::setGenreFilter(const QString& genre)
{
    beginFilterChange();
    m_genreFilter = genre;
    endFilterChange();
}

void GenreFilterProxy::setSearchText(const QString& text)
{
    beginFilterChange();
    m_searchText = text.trimmed();
    endFilterChange();
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

    // Scroll
    setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);

    // Selection
    setSelectionMode(QAbstractItemView::SingleSelection);
    setSelectionBehavior(QAbstractItemView::SelectRows);

    // Visual
    setShowGrid(false);
    setAlternatingRowColors(false);
    setMouseTracking(true);
    viewport()->setMouseTracking(true);
    verticalHeader()->hide();
    verticalHeader()->setDefaultSectionSize(54);
    setSortingEnabled(true);

    // Column widths (DESIGN_SPEC §3)
    auto* hdr = horizontalHeader();
    hdr->setSectionResizeMode(TrackModel::ColChevron, QHeaderView::Fixed);
    hdr->setSectionResizeMode(TrackModel::ColTitle,   QHeaderView::Stretch);
    hdr->setSectionResizeMode(TrackModel::ColBpm,     QHeaderView::Fixed);
    hdr->setSectionResizeMode(TrackModel::ColKey,     QHeaderView::Fixed);
    hdr->setSectionResizeMode(TrackModel::ColTime,    QHeaderView::Fixed);
    hdr->setSectionResizeMode(TrackModel::ColFormat,  QHeaderView::Fixed);

    setColumnWidth(TrackModel::ColChevron, 20);
    setColumnWidth(TrackModel::ColBpm,     72);
    setColumnWidth(TrackModel::ColKey,     64);
    setColumnWidth(TrackModel::ColTime,    72);
    setColumnWidth(TrackModel::ColFormat,  80);

    hdr->setFixedHeight(36);

    // Override system selection color (DESIGN_SPEC §3)
    QPalette p = palette();
    p.setColor(QPalette::Highlight,       QColor(0x0d, 0x3b, 0x4f));
    p.setColor(QPalette::HighlightedText, QColor(0xd0, 0xd0, 0xd0));
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

    // Collapse any previously expanded row
    if (m_expandedProxyRow != -1 && m_expandedProxyRow != proxyRow) {
        const QModelIndex prevProxy  = m_proxy->index(m_expandedProxyRow, 0);
        const QModelIndex prevSource = m_proxy->mapToSource(prevProxy);
        if (prevSource.isValid())
            m_trackModel->setExpanded(prevSource.row(), false);
    }

    m_trackModel->setExpanded(srcRow, nowExpanded);
    m_expandedProxyRow = nowExpanded ? proxyRow : -1;

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

void TrackTableView::mousePressEvent(QMouseEvent* event)
{
    const QModelIndex proxyIdx = indexAt(event->pos());
    if (proxyIdx.isValid() && proxyIdx.column() == TrackModel::ColChevron) {
        toggleRow(proxyIdx.row());
        return;
    }
    QTableView::mousePressEvent(event);
}

void TrackTableView::mouseMoveEvent(QMouseEvent* event)
{
    const QModelIndex proxyIdx = indexAt(event->pos());
    const int row = proxyIdx.isValid() ? proxyIdx.row() : -1;
    if (row != m_hoveredRow) {
        m_hoveredRow = row;
        viewport()->update();
    }
    QTableView::mouseMoveEvent(event);
}

void TrackTableView::leaveEvent(QEvent* event)
{
    m_hoveredRow = -1;
    viewport()->update();
    QTableView::leaveEvent(event);
}
