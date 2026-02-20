#pragma once

#include <QWidget>

class QSlider;
class QPushButton;
class QLabel;

#ifdef HAVE_MULTIMEDIA
#include <QMediaPlayer>
#include <QAudioOutput>

// PlayerBar — a minimal audio player strip docked at the bottom of the library.
// Controls: play/pause, seek slider with time, volume slider.
class PlayerBar : public QWidget
{
    Q_OBJECT
public:
    explicit PlayerBar(QWidget* parent = nullptr);

    // Load a file and start playing immediately.
    void playFile(const QString& filePath,
                  const QString& title,
                  const QString& artist);

private slots:
    void onPlayPauseClicked();
    void onPositionChanged(qint64 pos);
    void onDurationChanged(qint64 dur);
    void onSeekPressed();
    void onSeekReleased();
    void onSeekMoved(int value);
    void onVolumeChanged(int value);
    void onPlaybackStateChanged(QMediaPlayer::PlaybackState state);

private:
    static QString formatTime(qint64 ms);

    QMediaPlayer* m_player      = nullptr;
    QAudioOutput* m_audioOutput = nullptr;

    QPushButton*  m_playBtn     = nullptr;
    QSlider*      m_seekSlider  = nullptr;
    QSlider*      m_volSlider   = nullptr;
    QLabel*       m_trackLabel  = nullptr;
    QLabel*       m_timeLabel   = nullptr;

    bool          m_seeking     = false;
};

#else // Stub when Qt Multimedia is not available

class PlayerBar : public QWidget
{
    Q_OBJECT
public:
    explicit PlayerBar(QWidget* parent = nullptr) : QWidget(parent) { hide(); }
    void playFile(const QString&, const QString&, const QString&) {}
};

#endif // HAVE_MULTIMEDIA
