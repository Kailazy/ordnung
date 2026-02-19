#pragma once

#include <QTableView>
#include <QSortFilterProxyModel>

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

    void setSearchText(const QString& text);
    void setGenreFilter(const QString& genre);

    // Expand/collapse the detail panel for the clicked row.
    // Returns the track data QVariantMap for the expanded row, or empty if collapsed.
    QVariantMap toggleRow(int proxyRow);

    // Returns all currently visible (proxy) rows as FormatSnapshot-compatible data.
    QVector<QPair<int, long long>> visibleTrackIds() const;

signals:
    void trackExpanded(const QVariantMap& trackData);
    void trackCollapsed();
    void formatChangeRequested(const QModelIndex& sourceIndex, const QString& newFormat);

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    TrackModel*      m_trackModel;
    GenreFilterProxy* m_proxy;
    FormatDelegate*  m_delegate;
    QUndoStack*      m_undoStack;
    int              m_expandedProxyRow = -1;
    int              m_hoveredRow       = -1;
};
