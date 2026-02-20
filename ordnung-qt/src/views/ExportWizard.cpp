#include "ExportWizard.h"

#include "services/Database.h"
#include "services/ExportService.h"
#include "style/Theme.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QStackedWidget>
#include <QScrollArea>
#include <QPushButton>
#include <QLineEdit>
#include <QLabel>
#include <QProgressBar>
#include <QCheckBox>
#include <QRadioButton>
#include <QButtonGroup>
#include <QFileDialog>
#include <QMessageBox>
#include <QTimer>
#include <QDir>
#include <QStyle>
#include <QApplication>

// Update child label colors on a type card when selection changes.
// QSS descendant selectors (QPushButton#exportTypeCard[selected] QLabel) are
// unreliable in some Qt versions, so we set inline styles as a safe fallback.
static void updateCardLabelColors(QPushButton* card, bool selected)
{
    auto* titleLbl = card->findChild<QLabel*>(QStringLiteral("exportCardTitle"));
    auto* descLbl  = card->findChild<QLabel*>(QStringLiteral("exportCardDesc"));
    if (titleLbl)
        titleLbl->setStyleSheet(selected ? QStringLiteral("color: %1;").arg(Theme::Color::Accent)
                                         : QStringLiteral("color: %1;").arg(Theme::Color::Text));
    if (descLbl)
        descLbl->setStyleSheet(selected ? QStringLiteral("color: #8a4a3a;")
                                        : QStringLiteral("color: %1;").arg(Theme::Color::Text2));
}

// ─────────────────────────────────────────────────────────────────────────────
// Construction
// ─────────────────────────────────────────────────────────────────────────────

ExportWizard::ExportWizard(Database* db, QWidget* parent)
    : QDialog(parent)
    , m_db(db)
{
    setObjectName(QStringLiteral("exportWizard"));
    setWindowTitle(QStringLiteral("Export"));
    setFixedSize(640, 520);
    setModal(true);

    m_exportService = new ExportService(db, this);

    // ── Root layout ──────────────────────────────────────────────────────────
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    root->addWidget(buildHeader());

    m_pages = new QStackedWidget(this);
    m_pages->addWidget(buildPage1());
    m_pages->addWidget(buildPage2());
    root->addWidget(m_pages, 1);

    root->addWidget(buildFooter());

    // Progress strip — sits outside the footer as the last root child
    m_progressStrip = new QProgressBar(this);
    m_progressStrip->setObjectName(QStringLiteral("exportProgressStrip"));
    m_progressStrip->setFixedHeight(4);
    m_progressStrip->setTextVisible(false);
    m_progressStrip->setRange(0, 100);
    m_progressStrip->setValue(0);
    m_progressStrip->setVisible(false);
    root->addWidget(m_progressStrip);

    // ── Connections: export service ──────────────────────────────────────────
    connect(m_exportService, &ExportService::progress,
            this, [this](ExportProgress p) {
                onExportProgress(p.done, p.total, p.currentFile);
            });
    connect(m_exportService, &ExportService::finished,
            this, &ExportWizard::onExportFinished);

    setActivePage(0);
}

// ─────────────────────────────────────────────────────────────────────────────
// Public API
// ─────────────────────────────────────────────────────────────────────────────

void ExportWizard::preselectPlaylist(long long playlistId)
{
    m_preselectedPlaylistId = playlistId;
}

// ─────────────────────────────────────────────────────────────────────────────
// Widget builders
// ─────────────────────────────────────────────────────────────────────────────

QWidget* ExportWizard::buildHeader()
{
    auto* header = new QWidget(this);
    header->setObjectName(QStringLiteral("exportHeader"));
    header->setFixedHeight(64);

    auto* layout = new QVBoxLayout(header);
    layout->setContentsMargins(Theme::Layout::ContentPadH, Theme::Layout::ButtonPadV + Theme::Layout::PadSm,
                               Theme::Layout::ContentPadH, Theme::Layout::ContentPadV);
    layout->setSpacing(Theme::Layout::GapXs);

    // Step indicator — monospace, uppercase, muted
    m_stepLabel = new QLabel(QStringLiteral("01  TARGET"), header);
    m_stepLabel->setObjectName(QStringLiteral("exportStep"));
    QFont stepFont(Theme::Font::Mono, Theme::Font::Small);
    stepFont.setLetterSpacing(QFont::AbsoluteSpacing, 3);
    m_stepLabel->setFont(stepFont);

    // Section title
    m_sectionTitle = new QLabel(QStringLiteral("Select export format"), header);
    m_sectionTitle->setObjectName(QStringLiteral("exportSectionTitle"));
    QFont titleFont = QApplication::font();
    titleFont.setPointSize(Theme::Font::Body);
    m_sectionTitle->setFont(titleFont);

    layout->addWidget(m_stepLabel);
    layout->addWidget(m_sectionTitle);

    return header;
}

QWidget* ExportWizard::buildPage1()
{
    auto* page = new QWidget(this);
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(Theme::Layout::ContentPadH, Theme::Layout::ContentPadH,
                               Theme::Layout::ContentPadH, Theme::Layout::ContentPadH);
    layout->setSpacing(Theme::Layout::GapLg);

    // ── Type cards ───────────────────────────────────────────────────────────
    auto* cardsRow = new QHBoxLayout();
    cardsRow->setSpacing(Theme::Layout::GapLg);

    // Helper to build a type card with inner title + description labels
    auto makeCard = [this, page](const QString& title, const QString& desc) -> QPushButton* {
        auto* card = new QPushButton(page);
        card->setObjectName(QStringLiteral("exportTypeCard"));
        card->setCursor(Qt::PointingHandCursor);
        card->setMinimumHeight(140);
        card->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

        auto* cardLayout = new QVBoxLayout(card);
        cardLayout->setContentsMargins(Theme::Layout::ContentPadH, Theme::Layout::PadLg + Theme::Layout::PadXs,
                                       Theme::Layout::ContentPadH, Theme::Layout::PadLg + Theme::Layout::PadXs);
        cardLayout->setSpacing(Theme::Layout::PadSm);

        auto* titleLbl = new QLabel(title, card);
        titleLbl->setObjectName(QStringLiteral("exportCardTitle"));
        QFont tFont(Theme::Font::Mono, Theme::Font::Body);
        tFont.setWeight(QFont::Bold);
        tFont.setCapitalization(QFont::AllUppercase);
        titleLbl->setFont(tFont);

        auto* descLbl = new QLabel(desc, card);
        descLbl->setObjectName(QStringLiteral("exportCardDesc"));
        QFont dFont = QApplication::font();
        dFont.setPointSize(Theme::Font::Caption);
        descLbl->setFont(dFont);

        cardLayout->addWidget(titleLbl);
        cardLayout->addWidget(descLbl);
        cardLayout->addStretch();

        return card;
    };

    m_xmlCard = makeCard(QStringLiteral("Rekordbox XML"),
                         QStringLiteral("Export rekordbox.xml for Rekordbox software import. Use this to manage your library within Rekordbox."));
    m_xmlCard->setProperty("selected", true);
    updateCardLabelColors(m_xmlCard, true);

    m_usbCard = makeCard(QStringLiteral("CDJ USB Export"),
                         QStringLiteral("Write Pioneer export.pdb directly to USB — no Rekordbox required. CDJ-2000NXS2 and CDJ-3000 compatible."));
    m_usbCard->setProperty("selected", false);
    updateCardLabelColors(m_usbCard, false);

    cardsRow->addWidget(m_xmlCard);
    cardsRow->addWidget(m_usbCard);
    layout->addLayout(cardsRow);

    // Card mutual exclusion + inline label color update (QSS descendant fallback)
    connect(m_xmlCard, &QPushButton::clicked, this, [this] {
        m_xmlCard->setProperty("selected", true);
        m_usbCard->setProperty("selected", false);
        m_xmlCard->style()->unpolish(m_xmlCard); m_xmlCard->style()->polish(m_xmlCard);
        m_usbCard->style()->unpolish(m_usbCard); m_usbCard->style()->polish(m_usbCard);
        updateCardLabelColors(m_xmlCard, true);
        updateCardLabelColors(m_usbCard, false);
        m_pathLabel->setText(QStringLiteral("XML output path"));
        m_pathEdit->setPlaceholderText(QStringLiteral("select .xml destination..."));
        m_copyFilesCheck->setVisible(false);
    });
    connect(m_usbCard, &QPushButton::clicked, this, [this] {
        m_usbCard->setProperty("selected", true);
        m_xmlCard->setProperty("selected", false);
        m_xmlCard->style()->unpolish(m_xmlCard); m_xmlCard->style()->polish(m_xmlCard);
        m_usbCard->style()->unpolish(m_usbCard); m_usbCard->style()->polish(m_usbCard);
        updateCardLabelColors(m_usbCard, true);
        updateCardLabelColors(m_xmlCard, false);
        m_pathLabel->setText(QStringLiteral("USB mount point"));
        m_pathEdit->setPlaceholderText(QStringLiteral("select USB drive..."));
        m_copyFilesCheck->setVisible(true);
    });

    // ── Path row ─────────────────────────────────────────────────────────────
    m_pathLabel = new QLabel(QStringLiteral("XML output path"), page);
    QFont capFont = QApplication::font();
    capFont.setPointSize(Theme::Font::Caption);
    m_pathLabel->setFont(capFont);
    layout->addWidget(m_pathLabel);

    auto* pathRow = new QHBoxLayout();
    pathRow->setSpacing(Theme::Layout::GapSm);

    m_pathEdit = new QLineEdit(page);
    m_pathEdit->setObjectName(QStringLiteral("exportPathInput"));
    m_pathEdit->setPlaceholderText(QStringLiteral("select .xml destination..."));

    m_browseBtn = new QPushButton(QStringLiteral("BROWSE"), page);
    m_browseBtn->setObjectName(QStringLiteral("exportBrowseBtn"));
    m_browseBtn->setFixedWidth(80);
    m_browseBtn->setCursor(Qt::PointingHandCursor);
    connect(m_browseBtn, &QPushButton::clicked, this, &ExportWizard::onBrowseClicked);

    pathRow->addWidget(m_pathEdit, 1);
    pathRow->addWidget(m_browseBtn);
    layout->addLayout(pathRow);

    layout->addStretch();
    return page;
}

QWidget* ExportWizard::buildPage2()
{
    auto* page = new QWidget(this);
    auto* layout = new QHBoxLayout(page);
    layout->setContentsMargins(Theme::Layout::ContentPadH, Theme::Layout::ContentPadV,
                               Theme::Layout::ContentPadH, Theme::Layout::ContentPadV);
    layout->setSpacing(Theme::Layout::Pad2Xl);

    QFont colHeaderFont(Theme::Font::Mono, Theme::Font::Small);
    colHeaderFont.setLetterSpacing(QFont::AbsoluteSpacing, 2);
    colHeaderFont.setCapitalization(QFont::AllUppercase);

    // ── Left column: playlist selection ──────────────────────────────────────
    auto* leftCol = new QVBoxLayout();
    leftCol->setSpacing(Theme::Layout::GapSm);

    auto* plHeader = new QLabel(QStringLiteral("PLAYLISTS"), page);
    plHeader->setObjectName(QStringLiteral("exportColHeader"));
    plHeader->setFont(colHeaderFont);
    leftCol->addWidget(plHeader);

    m_playlistScroll = new QScrollArea(page);
    m_playlistScroll->setFrameShape(QFrame::NoFrame);
    m_playlistScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_playlistScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_playlistScroll->setWidgetResizable(true);

    m_playlistListWidget = new QWidget(m_playlistScroll);
    m_playlistListWidget->setObjectName(QStringLiteral("exportPlaylistScroll"));
    auto* plLayout = new QVBoxLayout(m_playlistListWidget);
    plLayout->setContentsMargins(Theme::Layout::PadSm, Theme::Layout::PadSm,
                                 Theme::Layout::PadSm, Theme::Layout::PadSm);
    plLayout->setSpacing(Theme::Layout::GapXs);

    // Populate playlist checkboxes from DB
    const QVector<Playlist> playlists = m_db->loadPlaylists();
    for (const Playlist& p : playlists) {
        const QString label = QString::fromStdString(p.name)
            + QStringLiteral("  (") + QString::number(p.total) + QLatin1Char(')');
        auto* cb = new QCheckBox(label, m_playlistListWidget);
        cb->setProperty("playlistId", static_cast<qlonglong>(p.id));
        cb->setChecked(m_preselectedPlaylistId <= 0 || p.id == m_preselectedPlaylistId);
        plLayout->addWidget(cb);
    }
    plLayout->addStretch();

    m_playlistScroll->setWidget(m_playlistListWidget);
    leftCol->addWidget(m_playlistScroll, 1);

    layout->addLayout(leftCol, 1);

    // ── Right column: output options ─────────────────────────────────────────
    auto* rightWrapper = new QWidget(page);
    rightWrapper->setFixedWidth(220);
    auto* rightCol = new QVBoxLayout(rightWrapper);
    rightCol->setContentsMargins(0, 0, 0, 0);
    rightCol->setSpacing(Theme::Layout::GapSm);

    auto* fmtHeader = new QLabel(QStringLiteral("OUTPUT FORMAT"), page);
    fmtHeader->setObjectName(QStringLiteral("exportColHeader"));
    fmtHeader->setFont(colHeaderFont);
    rightCol->addWidget(fmtHeader);

    m_formatGroup = new QButtonGroup(this);

    auto* keepRadio = new QRadioButton(QStringLiteral("Keep original"), rightWrapper);
    keepRadio->setChecked(true);
    m_formatGroup->addButton(keepRadio, 0);
    rightCol->addWidget(keepRadio);

    auto* aiffRadio = new QRadioButton(QStringLiteral("Convert to AIFF"), rightWrapper);
    m_formatGroup->addButton(aiffRadio, 1);
    rightCol->addWidget(aiffRadio);

    auto* wavRadio = new QRadioButton(QStringLiteral("Convert to WAV"), rightWrapper);
    m_formatGroup->addButton(wavRadio, 2);
    rightCol->addWidget(wavRadio);

    rightCol->addSpacing(Theme::Layout::GapLg);

    auto* fileHeader = new QLabel(QStringLiteral("FILE HANDLING"), page);
    fileHeader->setObjectName(QStringLiteral("exportColHeader"));
    fileHeader->setFont(colHeaderFont);
    rightCol->addWidget(fileHeader);

    m_copyFilesCheck = new QCheckBox(QStringLiteral("Copy files to destination"), rightWrapper);
    m_copyFilesCheck->setChecked(true);
    m_copyFilesCheck->setVisible(false);  // XML is default selection; copy files only applies to USB
    rightCol->addWidget(m_copyFilesCheck);

    rightCol->addStretch();

    auto* sizeHeader = new QLabel(QStringLiteral("ESTIMATED SIZE"), page);
    sizeHeader->setObjectName(QStringLiteral("exportColHeader"));
    sizeHeader->setFont(colHeaderFont);
    rightCol->addWidget(sizeHeader);

    m_sizeEstimate = new QLabel(QStringLiteral("\u2014"), rightWrapper);
    m_sizeEstimate->setObjectName(QStringLiteral("exportSizeEstimate"));
    QFont sizeFont = QApplication::font();
    sizeFont.setPointSize(Theme::Font::Body);
    m_sizeEstimate->setFont(sizeFont);
    rightCol->addWidget(m_sizeEstimate);

    layout->addWidget(rightWrapper);

    return page;
}

QWidget* ExportWizard::buildFooter()
{
    auto* footer = new QWidget(this);
    footer->setObjectName(QStringLiteral("exportFooter"));
    footer->setFixedHeight(56);

    auto* layout = new QHBoxLayout(footer);
    layout->setContentsMargins(Theme::Layout::ContentPadH, Theme::Layout::ContentPadV,
                               Theme::Layout::ContentPadH, Theme::Layout::ContentPadV);
    layout->setSpacing(Theme::Layout::GapSm);

    m_backBtn = new QPushButton(QStringLiteral("BACK"), footer);
    m_backBtn->setObjectName(QStringLiteral("exportBackBtn"));
    m_backBtn->setCursor(Qt::PointingHandCursor);
    connect(m_backBtn, &QPushButton::clicked, this, &ExportWizard::onBackClicked);

    layout->addWidget(m_backBtn);
    layout->addStretch();

    m_confirmBtn = new QPushButton(QStringLiteral("NEXT"), footer);
    m_confirmBtn->setObjectName(QStringLiteral("exportConfirmBtn"));
    m_confirmBtn->setProperty("btnStyle", "accent");
    m_confirmBtn->setCursor(Qt::PointingHandCursor);
    connect(m_confirmBtn, &QPushButton::clicked, this, &ExportWizard::onConfirmClicked);

    layout->addWidget(m_confirmBtn);

    return footer;
}

// ─────────────────────────────────────────────────────────────────────────────
// Navigation
// ─────────────────────────────────────────────────────────────────────────────

void ExportWizard::setActivePage(int page)
{
    m_pages->setCurrentIndex(page);

    if (page == 0) {
        m_stepLabel->setText(QStringLiteral("01  TARGET"));
        m_sectionTitle->setText(QStringLiteral("Select export format"));
        m_backBtn->setEnabled(false);
        m_confirmBtn->setText(QStringLiteral("NEXT"));
    } else {
        m_stepLabel->setText(QStringLiteral("02  OPTIONS"));
        m_sectionTitle->setText(m_xmlCard->property("selected").toBool()
                                ? QStringLiteral("Configure XML export")
                                : QStringLiteral("Configure USB export"));
        m_backBtn->setEnabled(true);
        m_confirmBtn->setText(QStringLiteral("EXPORT"));
    }
}

void ExportWizard::setExporting(bool exporting)
{
    m_progressStrip->setVisible(exporting);
    m_confirmBtn->setEnabled(!exporting);
    m_backBtn->setEnabled(!exporting);
    m_pages->setEnabled(!exporting);
}

// ─────────────────────────────────────────────────────────────────────────────
// Slots
// ─────────────────────────────────────────────────────────────────────────────

// Advance or start export depending on current page.
void ExportWizard::onConfirmClicked()
{
    if (m_pages->currentIndex() == 0) {
        if (m_pathEdit->text().trimmed().isEmpty()) {
            QMessageBox::warning(this, QStringLiteral("Export"),
                                 QStringLiteral("Please select an output path."));
            return;
        }
        setActivePage(1);
    } else {
        startExport();
    }
}

// Navigate back to step 1.
void ExportWizard::onBackClicked()
{
    setActivePage(0);
}

// Browse for output path (file for XML, directory for USB).
void ExportWizard::onBrowseClicked()
{
    const bool isXml = m_xmlCard->property("selected").toBool();
    if (isXml) {
        const QString file = QFileDialog::getSaveFileName(
            this,
            QStringLiteral("Export Rekordbox XML"),
            QDir::homePath() + QStringLiteral("/rekordbox.xml"),
            QStringLiteral("XML Files (*.xml)"));
        if (!file.isEmpty())
            m_pathEdit->setText(file);
    } else {
        const QString dir = QFileDialog::getExistingDirectory(
            this,
            QStringLiteral("Select USB Drive"),
            QDir::homePath(),
            QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
        if (!dir.isEmpty())
            m_pathEdit->setText(dir);
    }
}

void ExportWizard::startExport()
{
    const QString path = m_pathEdit->text().trimmed();
    if (path.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("Export"),
                             QStringLiteral("Please select an output path."));
        return;
    }

    // Collect selected playlist IDs
    QVector<long long> selectedIds;
    const auto children = m_playlistListWidget->findChildren<QCheckBox*>();
    for (const QCheckBox* cb : children) {
        if (cb->isChecked()) {
            const qlonglong id = cb->property("playlistId").toLongLong();
            if (id > 0)
                selectedIds.append(id);
        }
    }
    if (selectedIds.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("Export"),
                             QStringLiteral("Please select at least one playlist."));
        return;
    }

    ExportOptions opts;
    opts.target      = m_xmlCard->property("selected").toBool()
                         ? ExportOptions::RekordboxXml
                         : ExportOptions::CdjUsb;
    opts.outputPath  = path;
    opts.playlistIds = selectedIds;

    const int fmtId = m_formatGroup->checkedId();
    opts.outputFormat = (fmtId == 1) ? ExportOptions::ConvertToAiff
                      : (fmtId == 2) ? ExportOptions::ConvertToWav
                      : ExportOptions::KeepOriginal;

    opts.copyFiles = m_copyFilesCheck->isChecked();

    setExporting(true);
    m_progressStrip->setValue(0);
    m_exportService->startExport(opts);
}

// Handle export progress updates.
void ExportWizard::onExportProgress(int done, int total, const QString& /*currentFile*/)
{
    if (total > 0)
        m_progressStrip->setValue(static_cast<int>(100.0 * done / total));
}

// Handle export completion.
void ExportWizard::onExportFinished(bool success, const QString& errorMsg)
{
    setExporting(false);

    if (success) {
        m_progressStrip->setValue(100);
        m_progressStrip->setVisible(true);
        // Brief pause then close
        QTimer::singleShot(400, this, &QDialog::accept);
    } else {
        QMessageBox::critical(this, QStringLiteral("Export Failed"), errorMsg);
    }
}
