#include "FolderPanel.h"
#include "style/Theme.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QFileDialog>
#include <QApplication>
#include <QFontMetrics>

FolderPanel::FolderPanel(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("folderPanel"));
    setFixedWidth(Theme::Layout::PlaylistW);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // ── Header ────────────────────────────────────────────────────────────
    auto* header = new QWidget(this);
    header->setObjectName(QStringLiteral("folderPanelHeader"));
    auto* headerLayout = new QVBoxLayout(header);
    headerLayout->setContentsMargins(Theme::Layout::Pad, Theme::Layout::Pad,
                                     Theme::Layout::Pad, Theme::Layout::Pad);
    headerLayout->setSpacing(Theme::Layout::SpaceMd);

    auto* titleLabel = new QLabel(QStringLiteral("library"), header);
    titleLabel->setObjectName(QStringLiteral("folderPanelTitle"));
    titleLabel->setTextInteractionFlags(Qt::NoTextInteraction);
    QFont titleFont = QApplication::font();
    titleFont.setPointSize(17);
    titleLabel->setFont(titleFont);

    // Path display
    m_pathLabel = new QLabel(QStringLiteral("no folder set"), header);
    m_pathLabel->setObjectName(QStringLiteral("folderPath"));
    m_pathLabel->setWordWrap(false);
    m_pathLabel->setTextInteractionFlags(Qt::NoTextInteraction);
    {
        QFont f = QApplication::font();
        f.setPointSize(Theme::Font::Secondary);
        m_pathLabel->setFont(f);
    }

    // Browse button
    m_browseBtn = new QPushButton(QStringLiteral("set folder"), header);
    m_browseBtn->setObjectName(QStringLiteral("stdBtn"));
    m_browseBtn->setCursor(Qt::PointingHandCursor);
    connect(m_browseBtn, &QPushButton::clicked, this, &FolderPanel::onBrowseClicked);

    headerLayout->addWidget(titleLabel);
    headerLayout->addWidget(m_pathLabel);
    headerLayout->addWidget(m_browseBtn);

    layout->addWidget(header);
    layout->addStretch();
}

void FolderPanel::setFolder(const QString& path)
{
    m_folder = path;
    if (path.isEmpty()) {
        m_pathLabel->setText(QStringLiteral("no folder set"));
    } else {
        // Show just the last 2 path segments to save space
        const QStringList parts = path.split('/', Qt::SkipEmptyParts);
        const QString display = parts.size() > 2
            ? QStringLiteral("…/") + parts.at(parts.size() - 2) + QStringLiteral("/") + parts.last()
            : path;
        m_pathLabel->setText(display);
        m_pathLabel->setToolTip(path);
    }
}

void FolderPanel::onBrowseClicked()
{
    // Don't use this as parent: we're inside a Qt::Popup. When the dialog opens, focus
    // leaves the popup and it closes, which can hide or dismiss a child dialog on some platforms.
    const QString dir = QFileDialog::getExistingDirectory(
        nullptr,
        QStringLiteral("Select Library Folder"),
        m_folder.isEmpty() ? QDir::homePath() : m_folder,
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);

    if (!dir.isEmpty()) {
        setFolder(dir);
        emit folderChanged(dir);
    }
}
