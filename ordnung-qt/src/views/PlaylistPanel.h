#pragma once

#include <QWidget>
#include <QListView>
#include <QStyledItemDelegate>
#include <QLabel>
#include <QPoint>

class PlaylistModel;
class Database;

// ImportZone — drag-and-drop .txt target and click-to-browse area.
class ImportZone : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(bool dragActive READ dragActive WRITE setDragActive)
public:
    explicit ImportZone(QWidget* parent = nullptr);

    bool dragActive() const { return m_dragActive; }
    void setDragActive(bool active);

signals:
    void filesDropped(const QStringList& paths);
    void clicked();

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragLeaveEvent(QDragLeaveEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    bool m_dragActive = false;
    bool m_hovered    = false;
};

// PlaylistItemDelegate — paints each playlist row with name, track count, and
// a hover-reveal delete button. All painting via QPainter, no setCellWidget.
class PlaylistItemDelegate : public QStyledItemDelegate
{
    Q_OBJECT
public:
    explicit PlaylistItemDelegate(QObject* parent = nullptr);

    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override;
    QSize sizeHint(const QStyleOptionViewItem& option,
                   const QModelIndex& index) const override;

    void setHoveredRow(int row)        { m_hoveredRow = row; }
    void setDeleteHovered(bool hovered){ m_deleteHovered = hovered; }

    // Returns the delete button rect for the given row option rect.
    static QRect deleteRect(const QRect& itemRect);

signals:
    void deleteRequested(const QModelIndex& index);

private:
    int  m_hoveredRow    = -1;
    bool m_deleteHovered = false;
};

// PlaylistPanel — the left pane in the library splitter.
class PlaylistPanel : public QWidget
{
    Q_OBJECT
public:
    explicit PlaylistPanel(PlaylistModel* model, QWidget* parent = nullptr);

    void reload();

signals:
    void playlistSelected(long long id);
    void deleteRequested(long long id);
    void importRequested(const QStringList& filePaths);

public slots:
    void setActivePlaylist(long long id);

private slots:
    void onItemClicked(const QModelIndex& index);
    void onDeleteRequested(const QModelIndex& index);
    void onImportZoneClicked();
    void onImportZoneFilesDropped(const QStringList& paths);
    void onMouseMove(const QPoint& pos);

private:
    PlaylistModel*       m_model;
    QListView*           m_listView;
    PlaylistItemDelegate* m_delegate;
    ImportZone*          m_importZone;
    long long            m_activeId = -1;

    bool eventFilter(QObject* obj, QEvent* event) override;
};
