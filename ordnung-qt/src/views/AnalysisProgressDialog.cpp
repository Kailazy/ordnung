#include "AnalysisProgressDialog.h"

#include "services/AudioAnalyzer.h"
#include "style/Theme.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QTimer>
#include <QFontMetrics>
#include <QApplication>
#include <QFileInfo>

// ─────────────────────────────────────────────────────────────────────────────
// Construction
// ─────────────────────────────────────────────────────────────────────────────

AnalysisProgressDialog::AnalysisProgressDialog(AudioAnalyzer* analyzer, QWidget* parent)
    : QDialog(parent, Qt::Dialog | Qt::FramelessWindowHint)
    , m_analyzer(analyzer)
{
    setObjectName(QStringLiteral("analysisDialog"));
    setFixedSize(480, 200);
    setModal(true);
    setAttribute(Qt::WA_StyledBackground, true);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ── Header row ───────────────────────────────────────────────────────────
    auto* header = new QWidget(this);
    header->setObjectName(QStringLiteral("analysisHeader"));
    header->setFixedHeight(48);

    auto* headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(Theme::Layout::ContentPadH, 14,
                                     Theme::Layout::ContentPadH, 14);
    headerLayout->setSpacing(Theme::Layout::PadSm);

    auto* titleLabel = new QLabel(QStringLiteral("ANALYZING LIBRARY"), header);
    titleLabel->setObjectName(QStringLiteral("analysisTitle"));
    QFont titleFont(Theme::Font::Mono, Theme::Font::Small);
    titleFont.setLetterSpacing(QFont::AbsoluteSpacing, 3);
    titleLabel->setFont(titleFont);

    m_dotLabel = new QLabel(header);
    m_dotLabel->setObjectName(QStringLiteral("analysisDot"));
    m_dotLabel->setFixedSize(6, 6);
    // Start as active (accent color)
    m_dotLabel->setStyleSheet(QString("background-color: %1;").arg(Theme::Color::Accent));

    headerLayout->addWidget(titleLabel);
    headerLayout->addWidget(m_dotLabel);
    headerLayout->addStretch();

    root->addWidget(header);

    // ── Body ─────────────────────────────────────────────────────────────────
    auto* body = new QWidget(this);
    auto* bodyLayout = new QVBoxLayout(body);
    bodyLayout->setContentsMargins(Theme::Layout::ContentPadH, Theme::Layout::ContentPadV,
                                   Theme::Layout::ContentPadH, Theme::Layout::GapLg);
    bodyLayout->setSpacing(10);

    // Current filename (elided right)
    m_filenameLabel = new QLabel(QStringLiteral("preparing..."), body);
    m_filenameLabel->setObjectName(QStringLiteral("analysisFilename"));
    QFont fileFont = QApplication::font();
    fileFont.setPointSize(Theme::Font::Secondary);
    m_filenameLabel->setFont(fileFont);
    m_filenameLabel->setTextInteractionFlags(Qt::NoTextInteraction);
    m_filenameLabel->setMinimumWidth(0);
    m_filenameLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    bodyLayout->addWidget(m_filenameLabel);

    // Progress bar (6px)
    m_progressBar = new QProgressBar(body);
    m_progressBar->setObjectName(QStringLiteral("analysisProgress"));
    m_progressBar->setFixedHeight(6);
    m_progressBar->setTextVisible(false);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    bodyLayout->addWidget(m_progressBar);

    // Stats row: count left, elapsed right
    auto* statsRow = new QHBoxLayout();
    statsRow->setSpacing(0);

    m_countLabel = new QLabel(QStringLiteral("0 / 0"), body);
    m_countLabel->setObjectName(QStringLiteral("analysisStat"));
    QFont statFont(Theme::Font::Mono, Theme::Font::Caption);
    m_countLabel->setFont(statFont);

    m_elapsedLabel = new QLabel(QStringLiteral("0:00"), body);
    m_elapsedLabel->setObjectName(QStringLiteral("analysisStat"));
    m_elapsedLabel->setFont(statFont);

    statsRow->addWidget(m_countLabel);
    statsRow->addStretch();
    statsRow->addWidget(m_elapsedLabel);

    bodyLayout->addLayout(statsRow);

    root->addWidget(body, 1);

    // ── Footer ───────────────────────────────────────────────────────────────
    auto* footer = new QWidget(this);
    footer->setObjectName(QStringLiteral("analysisFooter"));
    footer->setFixedHeight(52);

    auto* footerLayout = new QHBoxLayout(footer);
    footerLayout->setContentsMargins(Theme::Layout::ContentPadH, Theme::Layout::ContentPadV,
                                     Theme::Layout::ContentPadH, Theme::Layout::ContentPadV);
    footerLayout->setSpacing(0);
    footerLayout->addStretch();

    m_cancelBtn = new QPushButton(QStringLiteral("CANCEL"), footer);
    m_cancelBtn->setObjectName(QStringLiteral("analysisCancelBtn"));
    m_cancelBtn->setProperty("btnStyle", "danger");
    m_cancelBtn->setCursor(Qt::PointingHandCursor);
    connect(m_cancelBtn, &QPushButton::clicked, this, &AnalysisProgressDialog::onCancelClicked);
    footerLayout->addWidget(m_cancelBtn);

    root->addWidget(footer);

    // ── Timers ───────────────────────────────────────────────────────────────

    // Elapsed time ticker (1s interval)
    m_tickTimer = new QTimer(this);
    m_tickTimer->setInterval(1000);
    connect(m_tickTimer, &QTimer::timeout, this, &AnalysisProgressDialog::onTimerTick);

    // Pulsing dot timer (400ms toggle)
    m_dotTimer = new QTimer(this);
    m_dotTimer->setInterval(400);
    connect(m_dotTimer, &QTimer::timeout, this, &AnalysisProgressDialog::onDotPulse);

    // ── Analyzer connections ─────────────────────────────────────────────────
    connect(m_analyzer, &AudioAnalyzer::progress,
            this, &AnalysisProgressDialog::onProgress);
    connect(m_analyzer, &AudioAnalyzer::finished,
            this, &AnalysisProgressDialog::onFinished);

    // Start timers
    m_elapsed.start();
    m_tickTimer->start();
    m_dotTimer->start();
}

// ─────────────────────────────────────────────────────────────────────────────
// Slots
// ─────────────────────────────────────────────────────────────────────────────

// Handle progress updates from AudioAnalyzer.
void AnalysisProgressDialog::onProgress(int done, int total, const QString& currentFile)
{
    m_doneTracks  = done;
    m_totalTracks = total;

    if (total > 0)
        m_progressBar->setValue(static_cast<int>(100.0 * done / total));

    // Show only the filename stem, elided to fit
    const QString stem = QFileInfo(currentFile).fileName();
    const QFontMetrics fm(m_filenameLabel->font());
    m_filenameLabel->setText(fm.elidedText(stem, Qt::ElideRight, m_filenameLabel->width()));

    m_countLabel->setText(QString("%1 / %2").arg(done).arg(total));
}

// Handle analysis completion.
void AnalysisProgressDialog::onFinished(const QVector<Track>& tracks)
{
    m_result = tracks;
    m_tickTimer->stop();
    m_dotTimer->stop();
    accept();
}

// Update the elapsed time display.
void AnalysisProgressDialog::onTimerTick()
{
    const qint64 secs = m_elapsed.elapsed() / 1000;
    const int mins = static_cast<int>(secs / 60);
    const int remSecs = static_cast<int>(secs % 60);
    m_elapsedLabel->setText(QString("%1:%2")
        .arg(mins)
        .arg(remSecs, 2, 10, QLatin1Char('0')));
}

// Toggle the pulsing dot between active (accent) and idle (muted) colors.
void AnalysisProgressDialog::onDotPulse()
{
    m_dotActive = !m_dotActive;
    m_dotLabel->setStyleSheet(
        m_dotActive
            ? QString("background-color: %1;").arg(Theme::Color::Accent)
            : QString("background-color: %1;").arg(Theme::Color::Text3));
}

// User clicked cancel.
void AnalysisProgressDialog::onCancelClicked()
{
    m_analyzer->cancel();
    m_cancelBtn->setEnabled(false);
    m_cancelBtn->setText(QStringLiteral("CANCELING..."));
    // The finished signal from the analyzer will close the dialog via onFinished()
}
