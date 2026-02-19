#include "PlaylistPanel.h"
#include "models/PlaylistModel.h"

#include <QPainter>
#include <QPainterPath>
#include <QVBoxLayout>
#include <QLabel>
#include <QListView>
#include <QScrollArea>
#include <QMouseEvent>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QFileDialog>
#include <QApplication>
#include <QFontMetrics>
#include <QStyle>
#include <QVariantMap>

// ════════════════════════════════════════════════════════════════════════════
// ImportZone
// ════════════════════════════════════════════════════════════════════════════

ImportZone::ImportZone(QWidget* parent)
    : QWidget(parent)
{
    setAcceptDrops(true);
    setCursor(Qt::PointingHandCursor);
    setMinimumHeight(64);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

void ImportZone::setDragActive(bool active)
{
    if (m_dragActive == active) return;
    m_dragActive = active;
    update();
}

void ImportZone::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    const QRectF r = QRectF(rect()).adjusted(1.5, 1.5, -1.5, -1.5);

    // Border color
    QColor borderColor;
    QColor textColor;

    if (m_dragActive) {
        borderColor = QColor(0x4f, 0xc3, 0xf7);  // active: accent
        textColor   = QColor(0x4f, 0xc3, 0xf7);
    } else if (m_hovered) {
        borderColor = QColor(0x44, 0x44, 0x44);  // hover: medium
        textColor   = QColor(0x77, 0x77, 0x77);
    } else {
        borderColor = QColor(0x1e, 0x1e, 0x1e);  // idle: barely visible
        textColor   = QColor(0x44, 0x44, 0x44);
    }

    // Draw dashed border rounded rect
    QPen pen(borderColor, 1.0, Qt::DashLine);
    pen.setDashPattern({4, 4});
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(r, 5, 5);

    // Draw centered text
    QFont f = QApplication::font();
    f.setPointSize(15);
    p.setFont(f);
    p.setPen(textColor);
    p.drawText(rect(), Qt::AlignCenter, QStringLiteral("drop rekordbox export"));
}

void ImportZone::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton)
        emit clicked();
}

void ImportZone::dragEnterEvent(QDragEnterEvent* event)
{
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
        setDragActive(true);
    }
}

void ImportZone::dragLeaveEvent(QDragLeaveEvent*)
{
    setDragActive(false);
}

void ImportZone::dropEvent(QDropEvent* event)
{
    setDragActive(false);
    QStringList paths;
    for (const QUrl& url : event->mimeData()->urls()) {
        const QString path = url.toLocalFile();
        if (path.endsWith(QLatin1String(".txt"), Qt::CaseInsensitive))
            paths.append(path);
    }
    if (!paths.isEmpty())
        emit filesDropped(paths);
}

void ImportZone::enterEvent(QEnterEvent*)
{
    m_hovered = true;
    update();
}

void ImportZone::leaveEvent(QEvent*)
{
    m_hovered = false;
    update();
}

// ════════════════════════════════════════════════════════════════════════════
// PlaylistItemDelegate
// ════════════════════════════════════════════════════════════════════════════

PlaylistItemDelegate::PlaylistItemDelegate(QObject* parent)
    : QStyledItemDelegate(parent)
{}

QRect PlaylistItemDelegate::deleteRect(const QRect& itemRect)
{
    // The delete "×" sits on the right edge of the row, 32px wide, centered vertically
    return QRect(itemRect.right() - 36, itemRect.top() + (itemRect.height() - 24) / 2, 28, 24);
}

void PlaylistItemDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option,
                                  const QModelIndex& index) const
{
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setRenderHint(QPainter::TextAntialiasing, true);

    const bool selected = option.state & QStyle::State_Selected;
    const bool hovered  = (index.row() == m_hoveredRow);

    // Row background
    if (selected && hovered) {
        painter->fillRect(option.rect, QColor(0x1c, 0x1c, 0x1c));
    } else if (selected) {
        painter->fillRect(option.rect, QColor(0x15, 0x15, 0x15));
    } else if (hovered) {
        painter->fillRect(option.rect, QColor(0x15, 0x15, 0x15));
    } else {
        painter->fillRect(option.rect, Qt::transparent);
    }

    // Left accent bar for selected
    if (selected) {
        painter->fillRect(QRect(option.rect.left(), option.rect.top(), 2, option.rect.height()),
                          QColor(0x4f, 0xc3, 0xf7));
    }

    // Name
    const QString name  = index.data(Qt::DisplayRole).toString();
    const int     total = index.data(PlaylistModel::TrackCountRole).toInt();

    const QRect contentRect = option.rect.adjusted(selected ? 14 : 12, 0, -44, 0);

    QFont nameFont = QApplication::font();
    nameFont.setPointSize(17);
    QFontMetrics nameFm(nameFont);

    QFont metaFont = QApplication::font();
    metaFont.setPointSize(15);
    QFontMetrics metaFm(metaFont);

    const int totalH = nameFm.height() + 3 + metaFm.height();
    const int startY = option.rect.top() + (option.rect.height() - totalH) / 2;

    // Format counts for meta line
    const QVariantMap fmtCounts = index.data(PlaylistModel::FormatCountsRole).toMap();
    QStringList fmtParts;
    // Sort by count descending
    QVector<QPair<int,QString>> sorted;
    for (auto it = fmtCounts.begin(); it != fmtCounts.end(); ++it)
        sorted.append({it.value().toInt(), it.key()});
    std::sort(sorted.begin(), sorted.end(), [](const auto& a, const auto& b){ return a.first > b.first; });
    for (const auto& [cnt, fmt] : sorted)
        fmtParts.append(QString::number(cnt) + QLatin1Char(' ') + fmt);

    QString metaText = QString::number(total) + QStringLiteral(" tracks");
    if (!fmtParts.isEmpty())
        metaText += QStringLiteral(" · ") + fmtParts.join(QStringLiteral(" · "));

    // Draw name
    painter->setFont(nameFont);
    painter->setPen(selected ? QColor(0xd0, 0xd0, 0xd0) : QColor(0xd0, 0xd0, 0xd0));
    painter->drawText(QRect(contentRect.left(), startY, contentRect.width(), nameFm.height()),
                      Qt::AlignLeft | Qt::AlignTop,
                      nameFm.elidedText(name, Qt::ElideRight, contentRect.width()));

    // Draw meta
    const int metaY = startY + nameFm.height() + 3;
    painter->setFont(metaFont);
    painter->setPen(selected ? QColor(0x77, 0x77, 0x77) : QColor(0x44, 0x44, 0x44));
    painter->drawText(QRect(contentRect.left(), metaY, contentRect.width(), metaFm.height()),
                      Qt::AlignLeft | Qt::AlignTop,
                      metaFm.elidedText(metaText, Qt::ElideRight, contentRect.width()));

    // Delete button (only visible on hover)
    if (hovered || selected) {
        const QRect delRect = deleteRect(option.rect);
        QFont delFont = QApplication::font();
        delFont.setPointSize(17);
        painter->setFont(delFont);

        QColor delColor;
        if (m_deleteHovered && hovered) {
            delColor = QColor(0xe5, 0x73, 0x73);
        } else if (selected || hovered) {
            delColor = QColor(0x44, 0x44, 0x44);
        } else {
            delColor = Qt::transparent;
        }
        painter->setPen(delColor);
        painter->drawText(delRect, Qt::AlignCenter, QStringLiteral("×"));
    }

    painter->restore();
}

QSize PlaylistItemDelegate::sizeHint(const QStyleOptionViewItem&, const QModelIndex&) const
{
    return QSize(0, 52);
}

// ════════════════════════════════════════════════════════════════════════════
// PlaylistPanel
// ════════════════════════════════════════════════════════════════════════════

PlaylistPanel::PlaylistPanel(PlaylistModel* model, QWidget* parent)
    : QWidget(parent)
    , m_model(model)
{
    setObjectName(QStringLiteral("playlistPanel"));
    setFixedWidth(320);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // ── Header ────────────────────────────────────────────────────────────
    auto* header = new QWidget(this);
    header->setObjectName(QStringLiteral("playlistHeader"));
    auto* headerLayout = new QVBoxLayout(header);
    headerLayout->setContentsMargins(23, 23, 23, 16);
    headerLayout->setSpacing(12);

    auto* titleLabel = new QLabel(QStringLiteral("playlists"), header);
    titleLabel->setObjectName(QStringLiteral("playlistTitle"));
    titleLabel->setTextInteractionFlags(Qt::NoTextInteraction);
    QFont titleFont = QApplication::font();
    titleFont.setPointSize(17);
    titleLabel->setFont(titleFont);

    m_importZone = new ImportZone(header);
    connect(m_importZone, &ImportZone::clicked, this, &PlaylistPanel::onImportZoneClicked);
    connect(m_importZone, &ImportZone::filesDropped, this, &PlaylistPanel::onImportZoneFilesDropped);

    headerLayout->addWidget(titleLabel);
    headerLayout->addWidget(m_importZone);
    layout->addWidget(header);

    // ── List view ─────────────────────────────────────────────────────────
    m_listView = new QListView(this);
    m_listView->setObjectName(QStringLiteral("playlistList"));
    m_listView->setModel(m_model);
    m_listView->setFrameShape(QFrame::NoFrame);
    m_listView->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_listView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_listView->setSelectionMode(QAbstractItemView::SingleSelection);
    m_listView->setMouseTracking(true);
    m_listView->setUniformItemSizes(true);
    m_listView->setSpacing(0);

    m_delegate = new PlaylistItemDelegate(this);
    m_listView->setItemDelegate(m_delegate);

    // Override QPalette selection color
    QPalette p = m_listView->palette();
    p.setColor(QPalette::Highlight,     QColor(0x15, 0x15, 0x15));
    p.setColor(QPalette::HighlightedText, QColor(0xd0, 0xd0, 0xd0));
    m_listView->setPalette(p);

    connect(m_listView, &QListView::clicked, this, &PlaylistPanel::onItemClicked);
    connect(m_delegate, &PlaylistItemDelegate::deleteRequested,
            this,       &PlaylistPanel::onDeleteRequested);

    m_listView->installEventFilter(this);
    m_listView->viewport()->installEventFilter(this);
    m_listView->viewport()->setMouseTracking(true);

    layout->addWidget(m_listView, 1);
}

void PlaylistPanel::reload()
{
    m_model->reload();
}

void PlaylistPanel::setActivePlaylist(long long id)
{
    m_activeId = id;

    // Select the matching row
    const int rows = m_model->rowCount();
    for (int i = 0; i < rows; ++i) {
        const QModelIndex idx = m_model->index(i);
        if (idx.data(PlaylistModel::PlaylistIdRole).toLongLong() == id) {
            m_listView->selectionModel()->select(idx, QItemSelectionModel::ClearAndSelect);
            return;
        }
    }
    m_listView->selectionModel()->clearSelection();
}

void PlaylistPanel::onItemClicked(const QModelIndex& index)
{
    const long long id = index.data(PlaylistModel::PlaylistIdRole).toLongLong();
    emit playlistSelected(id);
}

void PlaylistPanel::onDeleteRequested(const QModelIndex& index)
{
    const long long id = index.data(PlaylistModel::PlaylistIdRole).toLongLong();
    emit deleteRequested(id);
}

void PlaylistPanel::onImportZoneClicked()
{
    const QStringList paths = QFileDialog::getOpenFileNames(
        this,
        QStringLiteral("Import Rekordbox Export"),
        QString(),
        QStringLiteral("Text files (*.txt)"));
    if (!paths.isEmpty())
        emit importRequested(paths);
}

void PlaylistPanel::onImportZoneFilesDropped(const QStringList& paths)
{
    emit importRequested(paths);
}

void PlaylistPanel::onMouseMove(const QPoint& pos)
{
    const QModelIndex idx = m_listView->indexAt(pos);
    const int newRow = idx.isValid() ? idx.row() : -1;

    bool deleteHovered = false;
    if (idx.isValid()) {
        const QRect itemRect = m_listView->visualRect(idx);
        const QRect delRect  = PlaylistItemDelegate::deleteRect(itemRect);
        deleteHovered = delRect.contains(pos);
    }

    m_delegate->setHoveredRow(newRow);
    m_delegate->setDeleteHovered(deleteHovered);
    m_listView->viewport()->update();
}

bool PlaylistPanel::eventFilter(QObject* obj, QEvent* event)
{
    if (obj == m_listView->viewport()) {
        if (event->type() == QEvent::MouseMove) {
            auto* me = static_cast<QMouseEvent*>(event);
            onMouseMove(me->pos());
        } else if (event->type() == QEvent::Leave) {
            m_delegate->setHoveredRow(-1);
            m_delegate->setDeleteHovered(false);
            m_listView->viewport()->update();
        } else if (event->type() == QEvent::MouseButtonPress) {
            auto* me = static_cast<QMouseEvent*>(event);
            if (me->button() == Qt::LeftButton) {
                const QModelIndex idx = m_listView->indexAt(me->pos());
                if (idx.isValid()) {
                    const QRect itemRect = m_listView->visualRect(idx);
                    const QRect delRect  = PlaylistItemDelegate::deleteRect(itemRect);
                    if (delRect.contains(me->pos())) {
                        emit m_delegate->deleteRequested(idx);
                        return true;  // Consume event — do not select
                    }
                }
            }
        }
    }
    return QWidget::eventFilter(obj, event);
}
