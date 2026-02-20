#include "CollectionTreePanel.h"
#include "PlaylistPanel.h"          // re-use ImportZone
#include "services/Database.h"
#include "style/Theme.h"

#include <QVBoxLayout>
#include <QMenu>
#include <QAction>
#include <QHeaderView>
#include <QDebug>

// ─────────────────────────────────────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────────────────────────────────────

static QTreeWidgetItem* makeLeaf(const QString& label, int nodeType, long long id = -1)
{
    auto* item = new QTreeWidgetItem();
    item->setText(0, label);
    item->setData(0, Qt::UserRole,     nodeType);
    item->setData(0, Qt::UserRole + 1, static_cast<qlonglong>(id));
    item->setFlags(item->flags() & ~Qt::ItemIsDropEnabled);
    return item;
}

// ─────────────────────────────────────────────────────────────────────────────
// CollectionTreePanel
// ─────────────────────────────────────────────────────────────────────────────

CollectionTreePanel::CollectionTreePanel(Database* db, QWidget* parent)
    : QWidget(parent)
    , m_db(db)
{
    // ── Tree widget ──────────────────────────────────────────────────────────
    m_tree = new QTreeWidget(this);
    m_tree->setColumnCount(1);
    m_tree->header()->hide();
    m_tree->setRootIsDecorated(true);
    m_tree->setIndentation(16);
    m_tree->setAnimated(true);
    m_tree->setContextMenuPolicy(Qt::CustomContextMenu);
    m_tree->setObjectName("collectionTree");
    m_tree->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    // Accept drops for .txt Rekordbox export files onto the Playlists node
    m_tree->setAcceptDrops(true);
    m_tree->setDropIndicatorShown(true);

    // ── Import zone at bottom ────────────────────────────────────────────────
    m_importZone = new ImportZone(this);

    // ── Layout ───────────────────────────────────────────────────────────────
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(m_tree, 1);
    layout->addWidget(m_importZone);

    // ── Connections ──────────────────────────────────────────────────────────
    connect(m_tree, &QTreeWidget::itemClicked,
            this,   &CollectionTreePanel::onItemClicked);
    connect(m_tree, &QTreeWidget::customContextMenuRequested,
            this,   &CollectionTreePanel::onContextMenu);
    connect(m_importZone, &ImportZone::clicked,
            this,         &CollectionTreePanel::onImportZoneClicked);
    connect(m_importZone, &ImportZone::filesDropped,
            this,         &CollectionTreePanel::onImportZoneFilesDropped);

    buildTree();
}

// ─────────────────────────────────────────────────────────────────────────────
// Tree construction
// ─────────────────────────────────────────────────────────────────────────────

QTreeWidgetItem* CollectionTreePanel::makeCategory(const QString& label)
{
    auto* item = new QTreeWidgetItem(m_tree);
    item->setText(0, label);
    item->setData(0, NodeTypeRole, CategoryHeader);
    item->setFlags(Qt::ItemIsEnabled);  // not selectable
    item->setExpanded(true);

    // Style: small-caps muted colour
    QFont f = m_tree->font();
    f.setPointSize(Theme::Font::Small);
    f.setCapitalization(QFont::AllUppercase);
    f.setWeight(QFont::Medium);
    item->setFont(0, f);
    item->setForeground(0, QColor(Theme::Color::Text3));

    return item;
}

void CollectionTreePanel::buildTree()
{
    m_tree->clear();

    // ── Collection ───────────────────────────────────────────────────────────
    m_collectionNode = makeCategory(QStringLiteral("Collection"));

    auto* allTracks    = makeLeaf(QStringLiteral("All Tracks"),      AllTracks);
    auto* recentAdded  = makeLeaf(QStringLiteral("Recently Added"),  RecentlyAdded);
    auto* recentPlayed = makeLeaf(QStringLiteral("Recently Played"), RecentlyPlayed);
    m_collectionNode->addChild(allTracks);
    m_collectionNode->addChild(recentAdded);
    m_collectionNode->addChild(recentPlayed);

    // ── Playlists ────────────────────────────────────────────────────────────
    m_playlistsNode = makeCategory(QStringLiteral("Playlists"));

    // Load playlists from DB
    const QVector<Playlist> playlists = m_db->loadPlaylists();
    for (const Playlist& p : playlists) {
        const QString name = QString::fromStdString(p.name)
            + QStringLiteral("  (") + QString::number(p.total) + QLatin1Char(')');
        auto* pItem = makeLeaf(name, PlaylistNode, p.id);
        m_playlistsNode->addChild(pItem);
    }

    // "+ Create Playlist" action node
    auto* createItem = makeLeaf(QStringLiteral("+ new playlist"), CreatePlaylist);
    {
        QFont italicF = m_tree->font();
        italicF.setPointSize(Theme::Font::Caption);
        italicF.setItalic(true);
        createItem->setFont(0, italicF);
        createItem->setForeground(0, QColor(Theme::Color::Text3));
    }
    m_playlistsNode->addChild(createItem);

    // ── Smart Playlists ──────────────────────────────────────────────────────
    m_smartNode = makeCategory(QStringLiteral("Smart Playlists"));

    auto* needsAiff = makeLeaf(QStringLiteral("Needs AIFF"),        SmartPlaylist);
    needsAiff->setData(0, IdRole, QVariant("needs_aiff"));
    auto* highBpm   = makeLeaf(QStringLiteral("High BPM (>140)"),   SmartPlaylist);
    highBpm->setData(0, IdRole, QVariant("high_bpm"));
    auto* topRated  = makeLeaf(QStringLiteral("Top Rated (★★★+)"), SmartPlaylist);
    topRated->setData(0, IdRole, QVariant("top_rated"));

    m_smartNode->addChild(needsAiff);
    m_smartNode->addChild(highBpm);
    m_smartNode->addChild(topRated);

    // ── History ──────────────────────────────────────────────────────────────
    m_historyNode = makeCategory(QStringLiteral("History"));
    // History entries loaded lazily when expanded — empty for now

    // Select "All Tracks" by default
    m_tree->setCurrentItem(allTracks);
}

// ─────────────────────────────────────────────────────────────────────────────
// Public API
// ─────────────────────────────────────────────────────────────────────────────

void CollectionTreePanel::reloadPlaylists()
{
    if (!m_playlistsNode) return;

    // Remove all children
    while (m_playlistsNode->childCount() > 0)
        delete m_playlistsNode->takeChild(0);

    const QVector<Playlist> playlists = m_db->loadPlaylists();
    for (const Playlist& p : playlists) {
        const QString name = QString::fromStdString(p.name)
            + QStringLiteral("  (") + QString::number(p.total) + QLatin1Char(')');
        auto* pItem = makeLeaf(name, PlaylistNode, p.id);
        if (p.id == m_activePlaylistId) {
            QFont bold = m_tree->font();
            bold.setWeight(QFont::DemiBold);
            pItem->setFont(0, bold);
            pItem->setForeground(0, QColor(Theme::Color::Accent));
        }
        m_playlistsNode->addChild(pItem);
    }

    // Re-add the "create" leaf
    auto* createItem = makeLeaf(QStringLiteral("+ new playlist"), CreatePlaylist);
    {
        QFont italicF = m_tree->font();
        italicF.setPointSize(Theme::Font::Caption);
        italicF.setItalic(true);
        createItem->setFont(0, italicF);
        createItem->setForeground(0, QColor(Theme::Color::Text3));
    }
    m_playlistsNode->addChild(createItem);
}

void CollectionTreePanel::setActivePlaylist(long long id)
{
    m_activePlaylistId = id;
    reloadPlaylists();
}

// ─────────────────────────────────────────────────────────────────────────────
// Slots
// ─────────────────────────────────────────────────────────────────────────────

void CollectionTreePanel::onItemClicked(QTreeWidgetItem* item, int /*column*/)
{
    if (!item) return;

    const int nodeType = item->data(0, NodeTypeRole).toInt();
    const QVariant idVariant = item->data(0, IdRole);

    switch (nodeType) {
    case AllTracks:
    case RecentlyAdded:
    case RecentlyPlayed:
        emit collectionSelected();
        break;
    case PlaylistNode: {
        const long long id = idVariant.toLongLong();
        m_activePlaylistId = id;
        emit playlistSelected(id);
        break;
    }
    case SmartPlaylist:
        emit smartPlaylistSelected(idVariant.toString());
        break;
    case CreatePlaylist:
        emit createPlaylistRequested();
        break;
    case HistoryDate:
        emit historyDateSelected(idVariant.toString());
        break;
    case CategoryHeader:
        // Toggle expand/collapse on category header click
        item->setExpanded(!item->isExpanded());
        break;
    default:
        break;
    }
}

void CollectionTreePanel::onContextMenu(const QPoint& pos)
{
    QTreeWidgetItem* item = m_tree->itemAt(pos);
    if (!item) return;

    const int nodeType = item->data(0, NodeTypeRole).toInt();
    if (nodeType != PlaylistNode) return;

    const long long id = item->data(0, IdRole).toLongLong();

    QMenu menu(this);
    QAction* exportAct = menu.addAction(QStringLiteral("Export playlist..."));
    QAction* delAct    = menu.addAction(QStringLiteral("Delete Playlist"));
    QAction* chosen    = menu.exec(m_tree->viewport()->mapToGlobal(pos));
    if (chosen == exportAct)
        emit exportPlaylistRequested(id);
    else if (chosen == delAct)
        emit deletePlaylistRequested(id);
}

void CollectionTreePanel::onImportZoneClicked()
{
    emit importRequested({});
}

void CollectionTreePanel::onImportZoneFilesDropped(const QStringList& paths)
{
    emit importRequested(paths);
}
