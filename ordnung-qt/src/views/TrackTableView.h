#pragma once

#include <QTableView>
#include <QSortFilterProxyModel>
#include <QVector>

class TrackModel;
class FormatDelegate;
class QUndoStack;

// GenreFilterProxy extends QSortFilterProxyModel to support:
//   1. Text search across title, artist, album, genre, key columns
//   2. Genre tag filter (exact substring match within the genre field)
class GenreFilterProxy : public QSortFilterProxyModel
{
    Q_OBJECT
public:
    explicit GenreFilterProxy(QObject* parent = nullptr);

    void setGenreFilter(const QString& genre);
    QString genreFilter() const { return m_genreFilter; }

    void setSearchText(const QString& text);

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;

private:
    QString m_genreFilter;
    QString m_searchText;
};

// TrackTableView — configured QTableView for the track list.
// - All cell content painted by FormatDelegate (no setCellWidget)
// - QUndoStack integration for format changes
// - Emits rowExpanded/rowCollapsed to drive the detail panel
// - Mouse tracking for hover states in delegates
class TrackTableView : public QTableView
{
    Q_OBJECT
public:
    explicit TrackTableView(TrackModel* model, QUndoStack* undoStack,
                            QWidget* parent = nullptr);

    GenreFilterProxy* proxy() const { return m_proxy; }
    TrackModel*       trackModel() const { return m_trackModel; }

    void setSearchText(const QString& text);
    void setGenreFilter(const QString& genre);

    int  hoveredRow()   const { return m_hoveredRow; }
    bool hoveredThumb() const { return m_hoveredThumb; }

    // Expand/collapse the detail panel for the clicked row.
    // Returns the track data QVariantMap for the expanded row, or empty if collapsed.
    QVariantMap toggleRow(int proxyRow);

    // Returns all currently visible (proxy) rows as FormatSnapshot-compatible data.
    QVector<QPair<int, long long>> visibleTrackIds() const;

signals:
    void trackExpanded(const QVariantMap& trackData);
    void trackCollapsed();
    void formatChangeRequested(const QModelIndex& sourceIndex, const QString& newFormat);
    void playRequested(const QString& filePath, const QString& title, const QString& artist);
    // Emitted from context menu when user toggles preparation state.
    void prepareToggleRequested(long long songId, bool currentlyPrepared);
    // Emitted from context menu when user wants to batch-edit selected tracks.
    void batchEditRequested();

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;

private:
    void applyColumnVisibilityAndWidths();
    void onSectionResized(int logicalIndex, int oldSize, int newSize);
    static QRect thumbnailRect(const QRect& cellRect);
    void tryPlayFromThumbnail(const QModelIndex& proxyIdx, const QPoint& pos);

    QVector<int> m_columnWeights;
    QVector<int> m_columnMinWidths;
    // Stored column widths: on window resize only the last column changes (takes remainder); dragging a divider only moves that boundary (Rekordbox-style).
    QVector<int> m_columnWidths;

    TrackModel*       m_trackModel;
    GenreFilterProxy* m_proxy;
    FormatDelegate*   m_delegate;
    QUndoStack*       m_undoStack;
    long long         m_expandedTrackId = -1;
    int               m_hoveredRow      = -1;
    bool              m_hoveredThumb    = false;
};
