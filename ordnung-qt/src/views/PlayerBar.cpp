#include "PlayerBar.h"

#ifdef HAVE_MULTIMEDIA

#include "style/Theme.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QSlider>
#include <QPushButton>
#include <QLabel>
#include <QUrl>
#include <QApplication>
#include <QFont>

PlayerBar::PlayerBar(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("playerBar"));
    setFixedHeight(64);

    m_player      = new QMediaPlayer(this);
    m_audioOutput = new QAudioOutput(this);
    m_player->setAudioOutput(m_audioOutput);
    m_audioOutput->setVolume(0.7f);

    // ── Controls ─────────────────────────────────────────────────────────────
    m_playBtn = new QPushButton(QStringLiteral("▶"), this);
    m_playBtn->setObjectName(QStringLiteral("playerPlayBtn"));
    m_playBtn->setFixedSize(36, 36);
    m_playBtn->setCursor(Qt::PointingHandCursor);
    m_playBtn->setEnabled(false);

    m_trackLabel = new QLabel(QStringLiteral("—"), this);
    m_trackLabel->setObjectName(QStringLiteral("playerTrackLabel"));
    m_trackLabel->setTextInteractionFlags(Qt::NoTextInteraction);
    m_trackLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    {
        QFont f = QApplication::font();
        f.setPointSize(Theme::Font::Secondary);
        m_trackLabel->setFont(f);
    }

    m_seekSlider = new QSlider(Qt::Horizontal, this);
    m_seekSlider->setObjectName(QStringLiteral("playerSeekSlider"));
    m_seekSlider->setRange(0, 1000);
    m_seekSlider->setValue(0);
    m_seekSlider->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_seekSlider->setEnabled(false);

    m_timeLabel = new QLabel(QStringLiteral("0:00 / 0:00"), this);
    m_timeLabel->setObjectName(QStringLiteral("playerTimeLabel"));
    m_timeLabel->setTextInteractionFlags(Qt::NoTextInteraction);
    {
        QFont f = QApplication::font();
        f.setFamily(QStringLiteral("Consolas, Courier New, monospace"));
        f.setPointSize(Theme::Font::Secondary);
        m_timeLabel->setFont(f);
    }

    m_volSlider = new QSlider(Qt::Horizontal, this);
    m_volSlider->setObjectName(QStringLiteral("playerVolSlider"));
    m_volSlider->setRange(0, 100);
    m_volSlider->setValue(70);
    m_volSlider->setFixedWidth(80);

    // ── Layout ───────────────────────────────────────────────────────────────
    // Row 1: play button + track label
    // Row 2: seek slider + time + volume
    auto* col = new QVBoxLayout();
    col->setContentsMargins(0, 0, 0, 0);
    col->setSpacing(4);

    auto* row1 = new QHBoxLayout();
    row1->setContentsMargins(0, 0, 0, 0);
    row1->setSpacing(Theme::Layout::Gap);
    row1->addWidget(m_playBtn);
    row1->addWidget(m_trackLabel, 1);

    auto* row2 = new QHBoxLayout();
    row2->setContentsMargins(0, 0, 0, 0);
    row2->setSpacing(Theme::Layout::Gap);
    row2->addSpacing(36 + Theme::Layout::Gap);   // align under track label
    row2->addWidget(m_seekSlider, 1);
    row2->addWidget(m_timeLabel);
    row2->addSpacing(Theme::Layout::GapLg);
    row2->addWidget(m_volSlider);

    col->addLayout(row1);
    col->addLayout(row2);

    auto* root = new QHBoxLayout(this);
    root->setContentsMargins(Theme::Layout::ContentPadH, 0,
                             Theme::Layout::ContentPadH, 0);
    root->addLayout(col);

    // ── Connections ──────────────────────────────────────────────────────────
    connect(m_playBtn,  &QPushButton::clicked,
            this,       &PlayerBar::onPlayPauseClicked);

    connect(m_seekSlider, &QSlider::sliderPressed,
            this,         &PlayerBar::onSeekPressed);
    connect(m_seekSlider, &QSlider::sliderReleased,
            this,         &PlayerBar::onSeekReleased);
    connect(m_seekSlider, &QSlider::sliderMoved,
            this,         &PlayerBar::onSeekMoved);

    connect(m_volSlider, &QSlider::valueChanged,
            this,        &PlayerBar::onVolumeChanged);

    connect(m_player, &QMediaPlayer::positionChanged,
            this,     &PlayerBar::onPositionChanged);
    connect(m_player, &QMediaPlayer::durationChanged,
            this,     &PlayerBar::onDurationChanged);
    connect(m_player, &QMediaPlayer::playbackStateChanged,
            this,     &PlayerBar::onPlaybackStateChanged);
}

void PlayerBar::playFile(const QString& filePath,
                         const QString& title,
                         const QString& artist)
{
    m_player->setSource(QUrl::fromLocalFile(filePath));

    const QString display = artist.isEmpty()
        ? title
        : artist + QStringLiteral(" — ") + title;
    m_trackLabel->setText(display);

    m_seekSlider->setEnabled(true);
    m_playBtn->setEnabled(true);
    m_player->play();
}

void PlayerBar::onPlayPauseClicked()
{
    if (m_player->playbackState() == QMediaPlayer::PlayingState)
        m_player->pause();
    else
        m_player->play();
}

void PlayerBar::onPositionChanged(qint64 pos)
{
    if (m_seeking) return;
    const qint64 dur = m_player->duration();
    if (dur > 0)
        m_seekSlider->setValue(static_cast<int>((pos * 1000) / dur));
    m_timeLabel->setText(formatTime(pos) + QStringLiteral(" / ")
                         + formatTime(dur));
}

void PlayerBar::onDurationChanged(qint64 dur)
{
    m_timeLabel->setText(formatTime(m_player->position()) + QStringLiteral(" / ")
                         + formatTime(dur));
}

void PlayerBar::onSeekPressed()
{
    m_seeking = true;
}

void PlayerBar::onSeekReleased()
{
    m_seeking = false;
    const qint64 dur = m_player->duration();
    if (dur > 0)
        m_player->setPosition((static_cast<qint64>(m_seekSlider->value()) * dur) / 1000);
}

void PlayerBar::onSeekMoved(int value)
{
    const qint64 dur = m_player->duration();
    if (dur > 0) {
        const qint64 pos = (static_cast<qint64>(value) * dur) / 1000;
        m_timeLabel->setText(formatTime(pos) + QStringLiteral(" / ")
                             + formatTime(dur));
    }
}

void PlayerBar::onVolumeChanged(int value)
{
    m_audioOutput->setVolume(static_cast<float>(value) / 100.0f);
}

void PlayerBar::onPlaybackStateChanged(QMediaPlayer::PlaybackState state)
{
    m_playBtn->setText(state == QMediaPlayer::PlayingState
                       ? QStringLiteral("⏸") : QStringLiteral("▶"));
}

QString PlayerBar::formatTime(qint64 ms)
{
    if (ms < 0) ms = 0;
    const qint64 totalSec = ms / 1000;
    const qint64 min      = totalSec / 60;
    const qint64 sec      = totalSec % 60;
    return QString::number(min) + QStringLiteral(":") +
           QString::number(sec).rightJustified(2, QLatin1Char('0'));
}

#endif // HAVE_MULTIMEDIA
