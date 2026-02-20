#include "TrackDetailPanel.h"
#include "CuePointEditor.h"
#include "services/Database.h"
#include "style/Theme.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QScrollArea>
#include <QCheckBox>
#include <QApplication>
#include <QFont>
#include <QFrame>
#include <QStyle>

// ── Label factories ─────────────────────────────────────────────────────────

QLabel* TrackDetailPanel::makeFieldLabel(const QString& key, QWidget* parent)
{
    auto* lbl = new QLabel(key.toUpper(), parent);
    lbl->setTextInteractionFlags(Qt::NoTextInteraction);
    QFont f = QApplication::font();
    f.setPointSize(11);
    lbl->setFont(f);
    lbl->setStyleSheet(QStringLiteral("color: #444444;"));
    return lbl;
}

QLabel* TrackDetailPanel::makeValueLabel(const QString& value, QWidget* parent)
{
    auto* lbl = new QLabel(value, parent);
    lbl->setTextInteractionFlags(Qt::NoTextInteraction);
    QFont f = QApplication::font();
    f.setPointSize(13);
    lbl->setFont(f);
    lbl->setStyleSheet(QStringLiteral("color: #d0d0d0;"));
    lbl->setWordWrap(false);
    return lbl;
}

QString TrackDetailPanel::starsString(int rating)
{
    if (rating <= 0) return QString();
    return QString(qBound(0, rating, 5), QChar(0x2605));  // ★
}

// ── TrackDetailPanel ─────────────────────────────────────────────────────────

TrackDetailPanel::TrackDetailPanel(Database* db, QWidget* parent)
    : QWidget(parent)
    , m_db(db)
{
    setObjectName(QStringLiteral("trackDetailPanel"));
    setVisible(false);
    setMinimumHeight(Theme::Layout::DetailPanelMinH);

    // Root: vertical — [metadata row] + [cue points row]
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ── Metadata row ─────────────────────────────────────────────────────────
    auto* metaWidget = new QWidget(this);
    auto* outer = new QHBoxLayout(metaWidget);
    outer->setContentsMargins(Theme::Layout::PanelPad, Theme::Layout::ContentPadV,
                              Theme::Layout::PanelPad, Theme::Layout::ContentPadV);
    outer->setSpacing(Theme::Layout::Pad2Xl);

    // ── Left: metadata grid ──────────────────────────────────────────────
    auto* grid = new QGridLayout();
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setHorizontalSpacing(Theme::Layout::GapLg);
    grid->setVerticalSpacing(Theme::Layout::GapSm);
    grid->setColumnMinimumWidth(0, 60);

    int row = 0;
    auto addField = [&](QLabel*& keyLbl, QLabel*& valLbl, const QString& key) {
        keyLbl = makeFieldLabel(key, this);
        valLbl = makeValueLabel(QString(), this);
        grid->addWidget(keyLbl, row, 0, Qt::AlignRight | Qt::AlignVCenter);
        grid->addWidget(valLbl, row, 1, Qt::AlignLeft  | Qt::AlignVCenter);
        ++row;
    };

    addField(m_albumKey,  m_albumVal,  QStringLiteral("album"));
    addField(m_genreKey,  m_genreVal,  QStringLiteral("genre"));
    addField(m_bpmKey,    m_bpmVal,    QStringLiteral("bpm"));
    addField(m_keyKey,    m_keyVal,    QStringLiteral("key"));
    addField(m_timeKey,   m_timeVal,   QStringLiteral("time"));
    addField(m_ratingKey, m_ratingVal, QStringLiteral("rating"));
    addField(m_addedKey,  m_addedVal,  QStringLiteral("added"));

    outer->addLayout(grid);

    // ── Separator ──────────────────────────────────────────────────────────
    auto* sep = new QFrame(this);
    sep->setFrameShape(QFrame::VLine);
    sep->setStyleSheet(QStringLiteral("color: #1e1e1e;"));
    outer->addWidget(sep);

    // ── Right: play button, AIFF toggle + playlist chips ────────────────────
    auto* rightCol = new QVBoxLayout();
    rightCol->setContentsMargins(0, 0, 0, 0);
    rightCol->setSpacing(Theme::Layout::GapMd);

    m_playBtn = new QPushButton(QStringLiteral("▶ Play"), this);
    m_playBtn->setObjectName(QStringLiteral("detailPlayBtn"));
    m_playBtn->setProperty("btnStyle", "accent");
    m_playBtn->setCursor(Qt::PointingHandCursor);
    connect(m_playBtn, &QPushButton::clicked, this, [this] {
        if (!m_filepath.isEmpty())
            emit playRequested(m_filepath, m_title, m_artist);
    });
    rightCol->addWidget(m_playBtn, 0, Qt::AlignLeft);

    m_aiffBtn = new QPushButton(QStringLiteral("no aiff"), this);
    m_aiffBtn->setObjectName(QStringLiteral("aiffToggle"));
    m_aiffBtn->setCursor(Qt::PointingHandCursor);
    connect(m_aiffBtn, &QPushButton::clicked, this, &TrackDetailPanel::onAiffToggled);
    rightCol->addWidget(m_aiffBtn, 0, Qt::AlignLeft);

    // Playlist chips scroll area
    auto* plLabel = makeFieldLabel(QStringLiteral("playlists"), this);
    rightCol->addWidget(plLabel, 0, Qt::AlignLeft);

    m_chipsScroll = new QScrollArea(this);
    m_chipsScroll->setFrameShape(QFrame::NoFrame);
    m_chipsScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_chipsScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_chipsScroll->setWidgetResizable(true);

    m_chipsWidget = new QWidget(m_chipsScroll);
    m_chipsWidget->setLayout(new QVBoxLayout());
    static_cast<QVBoxLayout*>(m_chipsWidget->layout())->setContentsMargins(0,0,0,0);
    static_cast<QVBoxLayout*>(m_chipsWidget->layout())->setSpacing(Theme::Layout::GapXs);
    m_chipsScroll->setWidget(m_chipsWidget);

    rightCol->addWidget(m_chipsScroll, 1);
    outer->addLayout(rightCol, 1);

    root->addWidget(metaWidget);

    // ── Separator ─────────────────────────────────────────────────────────────
    auto* hSep = new QWidget(this);
    hSep->setFixedHeight(1);
    hSep->setStyleSheet(QString("background-color: %1;").arg(Theme::Color::Border));
    hSep->setAttribute(Qt::WA_StyledBackground, true);
    root->addWidget(hSep);

    // ── Cue point row ─────────────────────────────────────────────────────────
    m_cueEditor = new CuePointEditor(db, this);
    auto* cueWrapper = new QWidget(this);
    auto* cueWrapLayout = new QHBoxLayout(cueWrapper);
    cueWrapLayout->setContentsMargins(Theme::Layout::PanelPad, Theme::Layout::ContentPadV,
                                      Theme::Layout::PanelPad, Theme::Layout::ContentPadV);
    cueWrapLayout->addWidget(m_cueEditor);
    cueWrapLayout->addStretch();
    root->addWidget(cueWrapper);
}

void TrackDetailPanel::populate(const QVariantMap& trackData, long long activePlaylistId)
{
    m_songId   = trackData.value(QStringLiteral("id")).toLongLong();
    m_hasAiff  = trackData.value(QStringLiteral("has_aiff")).toBool();
    m_filepath = trackData.value(QStringLiteral("filepath")).toString();
    m_title    = trackData.value(QStringLiteral("title")).toString();
    m_artist   = trackData.value(QStringLiteral("artist")).toString();

    auto setText = [](QLabel* lbl, const QString& text) {
        lbl->setText(text);
        lbl->setVisible(!text.isEmpty());
        lbl->parentWidget()->findChild<QLabel*>();  // trigger parent update
    };

    // Helper to show/hide both key and value together
    auto setField = [](QLabel* keyLbl, QLabel* valLbl, const QString& text) {
        const bool visible = !text.isEmpty();
        keyLbl->setVisible(visible);
        valLbl->setText(text);
        valLbl->setVisible(visible);
    };

    setField(m_albumKey,  m_albumVal,  trackData.value(QStringLiteral("album")).toString());
    setField(m_genreKey,  m_genreVal,  trackData.value(QStringLiteral("genre")).toString());

    const double bpm = trackData.value(QStringLiteral("bpm")).toDouble();
    setField(m_bpmKey, m_bpmVal, bpm > 0 ? QString::number(static_cast<int>(bpm)) : QString());

    setField(m_keyKey,    m_keyVal,    trackData.value(QStringLiteral("key")).toString());
    setField(m_timeKey,   m_timeVal,   trackData.value(QStringLiteral("time")).toString());

    const int rating = trackData.value(QStringLiteral("rating")).toInt();
    setField(m_ratingKey, m_ratingVal, rating > 0 ? starsString(rating) : QString());

    setField(m_addedKey, m_addedVal, trackData.value(QStringLiteral("added")).toString());

    // AIFF toggle button
    m_aiffBtn->setText(m_hasAiff ? QStringLiteral("has aiff ✓") : QStringLiteral("no aiff"));
    m_aiffBtn->setProperty("aiffActive", m_hasAiff);
    m_aiffBtn->style()->unpolish(m_aiffBtn);
    m_aiffBtn->style()->polish(m_aiffBtn);

    // Playlist chips
    buildPlaylistChips(m_songId, activePlaylistId);

    // Cue points
    m_cueEditor->loadCues(m_songId);

    m_playBtn->setEnabled(!m_filepath.isEmpty());

    setVisible(true);
}

void TrackDetailPanel::clear()
{
    m_songId   = -1;
    m_filepath.clear();
    m_title.clear();
    m_artist.clear();
    m_cueEditor->clear();
    setVisible(false);
}

void TrackDetailPanel::onAiffToggled()
{
    m_hasAiff = !m_hasAiff;
    m_aiffBtn->setText(m_hasAiff ? QStringLiteral("has aiff ✓") : QStringLiteral("no aiff"));
    m_aiffBtn->setProperty("aiffActive", m_hasAiff);
    m_aiffBtn->style()->unpolish(m_aiffBtn);
    m_aiffBtn->style()->polish(m_aiffBtn);
    emit aiffToggled(m_songId, m_hasAiff);
}

void TrackDetailPanel::onPlaylistChipToggled(long long songId, long long playlistId, bool checked)
{
    emit playlistMembershipChanged(songId, playlistId, checked);
}

void TrackDetailPanel::buildPlaylistChips(long long songId, long long activePlaylistId)
{
    // Remove all existing chips
    QLayout* chipsLayout = m_chipsWidget->layout();
    QLayoutItem* item;
    while ((item = chipsLayout->takeAt(0)) != nullptr) {
        delete item->widget();
        delete item;
    }

    // Load memberships from DB
    const QVector<PlaylistMembership> memberships = m_db->getSongPlaylists(songId);

    for (const PlaylistMembership& m : memberships) {
        auto* chip = new QCheckBox(m.name, m_chipsWidget);
        chip->setChecked(m.member);
        chip->setObjectName(QStringLiteral("playlistChip"));

        if (m.id == activePlaylistId) {
            // Style the current playlist differently
            chip->setStyleSheet(QStringLiteral("color: #4fc3f7;"));
        }

        const long long pid = m.id;
        connect(chip, &QCheckBox::toggled, this, [this, songId, pid](bool checked) {
            onPlaylistChipToggled(songId, pid, checked);
        });

        static_cast<QVBoxLayout*>(chipsLayout)->addWidget(chip);
    }

    static_cast<QVBoxLayout*>(chipsLayout)->addStretch();
}
