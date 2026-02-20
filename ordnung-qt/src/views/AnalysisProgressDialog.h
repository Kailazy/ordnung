#pragma once

#include <QDialog>
#include <QElapsedTimer>
#include <QVector>

#include "core/Track.h"

class AudioAnalyzer;
class QLabel;
class QProgressBar;
class QPushButton;
class QTimer;

// AnalysisProgressDialog — compact modal dialog shown while AudioAnalyzer
// processes the library. Frameless, fixed 480x200. Displays a pulsing dot,
// current filename, progress bar, track count, elapsed time, and cancel button.
class AnalysisProgressDialog : public QDialog
{
    Q_OBJECT
public:
    explicit AnalysisProgressDialog(AudioAnalyzer* analyzer, QWidget* parent = nullptr);

    // Returns the updated tracks after analysis completes.
    const QVector<Track>& updatedTracks() const { return m_result; }

private slots:
    // Handle progress updates from AudioAnalyzer.
    void onProgress(int done, int total, const QString& currentFile);

    // Handle analysis completion.
    void onFinished(const QVector<Track>& tracks);

    // Update the elapsed time display.
    void onTimerTick();

    // Toggle the pulsing dot between active/idle colors.
    void onDotPulse();

    // User clicked cancel.
    void onCancelClicked();

private:
    AudioAnalyzer*  m_analyzer      = nullptr;
    QVector<Track>  m_result;

    QLabel*         m_dotLabel      = nullptr;
    QLabel*         m_filenameLabel = nullptr;
    QProgressBar*   m_progressBar   = nullptr;
    QLabel*         m_countLabel    = nullptr;
    QLabel*         m_elapsedLabel  = nullptr;
    QPushButton*    m_cancelBtn     = nullptr;

    QElapsedTimer   m_elapsed;
    QTimer*         m_tickTimer     = nullptr;
    QTimer*         m_dotTimer      = nullptr;
    bool            m_dotActive     = true;

    int             m_totalTracks   = 0;
    int             m_doneTracks    = 0;
};
