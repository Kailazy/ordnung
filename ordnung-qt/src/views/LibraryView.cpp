#include "views/LibraryView.h"

#include "views/PlaylistPanel.h"
#include "views/TrackTableView.h"
#include "views/TrackDetailPanel.h"
#include "models/PlaylistModel.h"
#include "models/TrackModel.h"
#include "commands/BulkFormatCommand.h"
#include "commands/UpdateFormatCommand.h"
#include "services/Database.h"
#include "core/Track.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QSplitter>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QScrollArea>
#include <QScrollBar>
#include <QUndoStack>

static QWidget* makeSep(QWidget* parent)
{
    auto* sep = new QWidget(parent);
    sep->setFixedHeight(1);
    sep->setStyleSheet("background-color: #1e1e1e;");
    sep->setAttribute(Qt::WA_StyledBackground, true);
    return sep;
}

LibraryView::LibraryView(PlaylistModel* playlists, TrackModel* tracks,
                         Database* db, QUndoStack* undoStack, QWidget* parent)
    : QWidget(parent)
    , m_playlistModel(playlists)
    , m_trackModel(tracks)
    , m_db(db)
    , m_undoStack(undoStack)
{
    // ── Toolbar ─────────────────────────────────────────────────────────────
    auto* toolbar = new QWidget(this);
    toolbar->setObjectName("toolbar");
    toolbar->setFixedHeight(64);

    m_searchEdit = new QLineEdit(toolbar);
    m_searchEdit->setObjectName("searchInput");
    m_searchEdit->setPlaceholderText("search tracks...");
    m_searchEdit->setClearButtonEnabled(true);
    m_searchEdit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    m_formatCombo = new QComboBox(toolbar);
    for (const char* fmt : {"mp3","flac","wav","aiff","alac","ogg","m4a","wma","aac"})
        m_formatCombo->addItem(fmt);

    m_applyBtn = new QPushButton("apply to all", toolbar);
    m_applyBtn->setObjectName("stdBtn");

    m_undoBtn = new QPushButton("undo", toolbar);
    m_undoBtn->setProperty("btnStyle", "undo");
    m_undoBtn->setVisible(false);

    m_statsLabel = new QLabel(toolbar);
    m_statsLabel->setObjectName("statsLabel");
    m_statsLabel->setTextInteractionFlags(Qt::NoTextInteraction);

    auto* toolbarLayout = new QHBoxLayout(toolbar);
    toolbarLayout->setContentsMargins(28, 0, 28, 0);
    toolbarLayout->setSpacing(14);
    toolbarLayout->addWidget(m_searchEdit, 1);
    toolbarLayout->addWidget(m_formatCombo);
    toolbarLayout->addWidget(m_applyBtn);
    toolbarLayout->addWidget(m_undoBtn);
    toolbarLayout->addStretch();
    toolbarLayout->addWidget(m_statsLabel);

    // ── Genre bar ───────────────────────────────────────────────────────────
    auto* genreBar = new QWidget(this);
    genreBar->setObjectName("genreBar");
    genreBar->setFixedHeight(48);

    // Scrollable container for genre buttons
    auto* genreContent = new QWidget;
    m_genreBarLayout = new QHBoxLayout(genreContent);
    m_genreBarLayout->setContentsMargins(28, 0, 28, 0);
    m_genreBarLayout->setSpacing(7);
    m_genreBarLayout->addStretch();   // trailing stretch; buttons inserted before it

    auto* genreScroll = new QScrollArea(genreBar);
    genreScroll->setWidget(genreContent);
    genreScroll->setWidgetResizable(true);
    genreScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    genreScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    genreScroll->setFrameShape(QFrame::NoFrame);

    auto* genreBarLayout = new QHBoxLayout(genreBar);
    genreBarLayout->setContentsMargins(0, 0, 0, 0);
    genreBarLayout->addWidget(genreScroll);

    // ── Panels ───────────────────────────────────────────────────────────────
    m_playlistPanel = new PlaylistPanel(playlists, this);
    m_trackTable    = new TrackTableView(tracks, undoStack, this);
    m_detailPanel   = new TrackDetailPanel(db, this);
    m_detailPanel->setVisible(false);

    auto* vSplitter = new QSplitter(Qt::Vertical, this);
    vSplitter->addWidget(m_trackTable);
    vSplitter->addWidget(m_detailPanel);
    vSplitter->setCollapsible(0, false);
    vSplitter->setCollapsible(1, true);
    vSplitter->setSizes({600, 200});
    vSplitter->setHandleWidth(1);

    auto* hSplitter = new QSplitter(Qt::Horizontal, this);
    hSplitter->addWidget(m_playlistPanel);
    hSplitter->addWidget(vSplitter);
    hSplitter->setCollapsible(0, false);
    hSplitter->setCollapsible(1, false);
    hSplitter->setHandleWidth(1);
    hSplitter->setSizes({320, 780});

    // ── Root layout ─────────────────────────────────────────────────────────
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);
    root->addWidget(toolbar);
    root->addWidget(makeSep(this));
    root->addWidget(genreBar);
    root->addWidget(makeSep(this));
    root->addWidget(hSplitter, 1);

    // ── Connections ─────────────────────────────────────────────────────────
    connect(m_searchEdit, &QLineEdit::textChanged,
            m_trackTable, &TrackTableView::setSearchText);

    connect(m_searchEdit, &QLineEdit::textChanged, this, [this]{ updateStats(); });

    connect(m_applyBtn, &QPushButton::clicked, this, &LibraryView::onBulkApply);

    connect(m_undoBtn, &QPushButton::clicked, m_undoStack, &QUndoStack::undo);

    connect(m_undoStack, &QUndoStack::canUndoChanged,
            this, &LibraryView::onUndoAvailable);

    connect(m_playlistPanel, &PlaylistPanel::playlistSelected,
            this, &LibraryView::onPlaylistSelected);

    connect(m_playlistPanel, &PlaylistPanel::deleteRequested,
            this, &LibraryView::deletePlaylistRequested);

    connect(m_playlistPanel, &PlaylistPanel::importRequested,
            this, &LibraryView::importRequested);

    connect(m_trackTable, &TrackTableView::trackExpanded,
            this, &LibraryView::onTrackExpanded);

    connect(m_trackTable, &TrackTableView::trackCollapsed,
            this, &LibraryView::onTrackCollapsed);

    connect(m_trackTable, &TrackTableView::formatChangeRequested,
            this, [this](const QModelIndex& srcIdx, const QString& fmt) {
                m_undoStack->push(new UpdateFormatCommand(m_trackModel, m_db, srcIdx, fmt));
            });

    connect(m_detailPanel, &TrackDetailPanel::aiffToggled,
            this, [this](long long songId, bool val) {
                const int row = m_trackModel->rowForId(songId);
                if (row >= 0) m_trackModel->setHasAiff(row, val);
            });
}

void LibraryView::loadPlaylist(long long playlistId)
{
    m_activePlaylistId = playlistId;
    m_trackModel->loadPlaylist(playlistId);
    m_detailPanel->clear();

    // Build genre bar from loaded tracks
    const auto& tracks = m_trackModel->tracks();
    QStringList genres;
    for (const Track& t : tracks) {
        const QString g = QString::fromStdString(t.genre).trimmed();
        if (!g.isEmpty() && !genres.contains(g, Qt::CaseInsensitive))
            genres.append(g);
    }
    genres.sort(Qt::CaseInsensitive);
    buildGenreBar(genres);

    m_activeGenre.clear();
    m_trackTable->setGenreFilter({});
    m_trackTable->setSearchText({});
    m_searchEdit->clear();
    updateStats();

    m_playlistPanel->setActivePlaylist(playlistId);
}

void LibraryView::reloadPlaylists()
{
    m_playlistModel->reload();
}

void LibraryView::buildGenreBar(const QStringList& genres)
{
    // Remove all genre buttons — trailing stretch is always last item
    while (m_genreBarLayout->count() > 1)
        delete m_genreBarLayout->takeAt(0)->widget();

    for (const QString& g : genres) {
        auto* btn = new QPushButton(g);
        btn->setObjectName("genreTag");
        btn->setCheckable(true);
        btn->setFocusPolicy(Qt::NoFocus);

        connect(btn, &QPushButton::toggled, this, [this, g, btn](bool checked) {
            // Uncheck all other genre buttons
            for (int i = 0; i < m_genreBarLayout->count() - 1; ++i) {
                auto* w = qobject_cast<QPushButton*>(
                    m_genreBarLayout->itemAt(i)->widget());
                if (w && w != btn)
                    w->setChecked(false);
            }
            m_activeGenre = checked ? g : QString();
            m_trackTable->setGenreFilter(m_activeGenre);
            updateStats();
        });

        // Insert before the trailing stretch
        m_genreBarLayout->insertWidget(m_genreBarLayout->count() - 1, btn);
    }
}

void LibraryView::updateStats()
{
    const int filtered = m_trackTable->proxy()->rowCount();
    const int total    = m_trackModel->rowCount();
    if (filtered == total)
        m_statsLabel->setText(QString("%1 tracks").arg(total));
    else
        m_statsLabel->setText(QString("%1 / %2").arg(filtered).arg(total));
}

void LibraryView::onPlaylistSelected(long long id)
{
    loadPlaylist(id);
}

void LibraryView::onBulkApply()
{
    if (m_activePlaylistId < 0) return;

    const QString newFormat = m_formatCombo->currentText();
    const auto visible = m_trackTable->visibleTrackIds();

    QVector<FormatSnapshot> snapshot;
    snapshot.reserve(visible.size());
    for (const auto& [row, id] : visible) {
        const QString oldFmt = QString::fromStdString(m_trackModel->tracks()[row].format);
        snapshot.append({row, id, oldFmt});
    }

    if (snapshot.isEmpty()) return;

    m_undoStack->push(new BulkFormatCommand(m_trackModel, m_db, snapshot, newFormat));
}

void LibraryView::onTrackExpanded(const QVariantMap& data)
{
    m_detailPanel->populate(data, m_activePlaylistId);
    m_detailPanel->setVisible(true);
}

void LibraryView::onTrackCollapsed()
{
    m_detailPanel->clear();
}

void LibraryView::onUndoAvailable(bool available)
{
    m_undoBtn->setVisible(available);
}
