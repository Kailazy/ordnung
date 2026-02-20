#include "views/MissingFilesDialog.h"

#include "services/Database.h"
#include "style/Theme.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTableWidget>
#include <QHeaderView>
#include <QPushButton>
#include <QLabel>
#include <QFileDialog>
#include <QMessageBox>

MissingFilesDialog::MissingFilesDialog(Database* db, QWidget* parent)
    : QDialog(parent)
    , m_db(db)
{
    setWindowTitle("Missing Files");
    setFixedSize(640, 480);
    setStyleSheet(
        QString("QDialog { background: %1; }").arg(Theme::Color::Bg1));

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(Theme::Layout::PadXl, Theme::Layout::PadXl,
                             Theme::Layout::PadXl, Theme::Layout::PadLg);
    root->setSpacing(Theme::Layout::GapMd);

    // ── Header ──────────────────────────────────────────────────────────────
    auto* headerRow = new QHBoxLayout;
    headerRow->setSpacing(Theme::Layout::GapSm);

    auto* title = new QLabel("MISSING FILES", this);
    title->setObjectName("missingFilesTitle");
    title->setStyleSheet(
        QString("font-family: '%1', '%2'; font-size: %3px; color: %4; "
                "letter-spacing: 3px; font-weight: bold;")
            .arg(Theme::Font::Mono, Theme::Font::MonoFallback)
            .arg(Theme::Font::Title)
            .arg(Theme::Color::Text));
    headerRow->addWidget(title);

    m_countLabel = new QLabel(this);
    m_countLabel->setStyleSheet(
        QString("background: %1; color: %2; font-size: %3px; "
                "padding: 2px 8px; font-family: '%4', '%5';")
            .arg(Theme::Color::AmberBg, Theme::Color::Amber)
            .arg(Theme::Font::Badge)
            .arg(Theme::Font::Mono, Theme::Font::MonoFallback));
    headerRow->addWidget(m_countLabel);
    headerRow->addStretch();

    root->addLayout(headerRow);

    // ── Table ───────────────────────────────────────────────────────────────
    m_table = new QTableWidget(this);
    m_table->setObjectName("missingFilesTable");
    m_table->setColumnCount(4);
    m_table->setHorizontalHeaderLabels({"Title", "Artist", "Path", ""});
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionMode(QAbstractItemView::NoSelection);
    m_table->verticalHeader()->hide();
    m_table->horizontalHeader()->setStretchLastSection(false);
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_table->setShowGrid(false);
    m_table->setAlternatingRowColors(false);
    m_table->setStyleSheet(
        QString("QTableWidget { background: %1; border: 1px solid %2; color: %3; "
                "font-size: %4px; }"
                "QHeaderView::section { background: %5; color: %6; border: none; "
                "padding: 4px 8px; font-size: %7px; font-family: '%8', '%9'; "
                "letter-spacing: 1px; }")
            .arg(Theme::Color::Bg, Theme::Color::Border, Theme::Color::Text)
            .arg(Theme::Font::Body)
            .arg(Theme::Color::Bg2, Theme::Color::Text2)
            .arg(Theme::Font::Caption)
            .arg(Theme::Font::Mono, Theme::Font::MonoFallback));
    root->addWidget(m_table, 1);

    // ── Footer ──────────────────────────────────────────────────────────────
    auto* footer = new QHBoxLayout;
    footer->setSpacing(Theme::Layout::GapSm);

    auto* removeAllBtn = new QPushButton("REMOVE ALL", this);
    removeAllBtn->setCursor(Qt::PointingHandCursor);
    removeAllBtn->setStyleSheet(
        QString("QPushButton { background: %1; border: 1px solid %2; color: %3; "
                "padding: %4px %5px; font-size: %6px; font-family: '%7', '%8'; "
                "letter-spacing: 1px; }"
                "QPushButton:hover { background: %9; border-color: %3; }")
            .arg(Theme::Color::Bg2, Theme::Color::Border, Theme::Color::Red)
            .arg(Theme::Layout::ButtonPadV).arg(Theme::Layout::ButtonPadH)
            .arg(Theme::Font::Secondary)
            .arg(Theme::Font::Mono, Theme::Font::MonoFallback)
            .arg(Theme::Color::RedBg));
    connect(removeAllBtn, &QPushButton::clicked, this, &MissingFilesDialog::onRemoveAllClicked);
    footer->addWidget(removeAllBtn);

    footer->addStretch();

    auto* closeBtn = new QPushButton("CLOSE", this);
    closeBtn->setCursor(Qt::PointingHandCursor);
    closeBtn->setStyleSheet(
        QString("QPushButton { background: %1; border: 1px solid %2; color: %3; "
                "padding: %4px %5px; font-size: %6px; font-family: '%7', '%8'; "
                "letter-spacing: 1px; }"
                "QPushButton:hover { border-color: %9; }")
            .arg(Theme::Color::Bg2, Theme::Color::Border, Theme::Color::Text2)
            .arg(Theme::Layout::ButtonPadV).arg(Theme::Layout::ButtonPadH)
            .arg(Theme::Font::Secondary)
            .arg(Theme::Font::Mono, Theme::Font::MonoFallback)
            .arg(Theme::Color::BorderHov));
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
    footer->addWidget(closeBtn);

    root->addLayout(footer);

    reload();
}

void MissingFilesDialog::reload()
{
    const QVector<Track> missing = m_db->findMissingTracks();
    m_countLabel->setText(QString("%1 missing").arg(missing.size()));
    m_countLabel->setVisible(!missing.isEmpty());

    m_table->setRowCount(missing.size());
    for (int row = 0; row < missing.size(); ++row) {
        const Track& t = missing[row];
        const long long songId = t.id;

        auto* titleItem = new QTableWidgetItem(QString::fromStdString(t.title));
        titleItem->setForeground(QColor(Theme::Color::Text));
        m_table->setItem(row, 0, titleItem);

        auto* artistItem = new QTableWidgetItem(QString::fromStdString(t.artist));
        artistItem->setForeground(QColor(Theme::Color::Text2));
        m_table->setItem(row, 1, artistItem);

        auto* pathItem = new QTableWidgetItem(QString::fromStdString(t.filepath));
        pathItem->setForeground(QColor(Theme::Color::Amber));
        m_table->setItem(row, 2, pathItem);

        // Action buttons cell
        auto* cellWidget = new QWidget(m_table);
        auto* cellLayout = new QHBoxLayout(cellWidget);
        cellLayout->setContentsMargins(2, 2, 2, 2);
        cellLayout->setSpacing(4);

        auto* locateBtn = new QPushButton("LOCATE", cellWidget);
        locateBtn->setObjectName("missingLocateBtn");
        locateBtn->setCursor(Qt::PointingHandCursor);
        locateBtn->setFixedHeight(24);
        locateBtn->setStyleSheet(
            QString("QPushButton { background: transparent; border: 1px solid %1; "
                    "color: %2; font-size: %3px; padding: 0 6px; "
                    "font-family: '%4', '%5'; }"
                    "QPushButton:hover { border-color: %2; }")
                .arg(Theme::Color::Border, Theme::Color::Accent)
                .arg(Theme::Font::Small)
                .arg(Theme::Font::Mono, Theme::Font::MonoFallback));
        connect(locateBtn, &QPushButton::clicked, this, [this, songId]() {
            onLocateClicked(songId);
        });
        cellLayout->addWidget(locateBtn);

        auto* removeBtn = new QPushButton("REMOVE", cellWidget);
        removeBtn->setObjectName("missingRemoveBtn");
        removeBtn->setCursor(Qt::PointingHandCursor);
        removeBtn->setFixedHeight(24);
        removeBtn->setStyleSheet(
            QString("QPushButton { background: transparent; border: 1px solid %1; "
                    "color: %2; font-size: %3px; padding: 0 6px; "
                    "font-family: '%4', '%5'; }"
                    "QPushButton:hover { border-color: %2; }")
                .arg(Theme::Color::Border, Theme::Color::Red)
                .arg(Theme::Font::Small)
                .arg(Theme::Font::Mono, Theme::Font::MonoFallback));
        connect(removeBtn, &QPushButton::clicked, this, [this, songId]() {
            onRemoveClicked(songId);
        });
        cellLayout->addWidget(removeBtn);

        m_table->setCellWidget(row, 3, cellWidget);
    }
}

void MissingFilesDialog::onLocateClicked(long long songId)
{
    const QString newPath = QFileDialog::getOpenFileName(
        this, "Locate File", QString(),
        "Audio Files (*.mp3 *.flac *.wav *.aiff *.m4a *.aac *.alac *.ogg);;All Files (*)");
    if (newPath.isEmpty())
        return;

    m_db->updateTrackFilepath(songId, newPath);
    reload();
    emit libraryChanged();
}

void MissingFilesDialog::onRemoveClicked(long long songId)
{
    m_db->deleteTrack(songId);
    reload();
    emit libraryChanged();
}

void MissingFilesDialog::onRemoveAllClicked()
{
    const int count = m_table->rowCount();
    if (count == 0)
        return;

    const auto answer = QMessageBox::warning(
        this, "Remove All Missing",
        QString("Remove all %1 missing tracks from the library?\n\n"
                "This cannot be undone.").arg(count),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);

    if (answer != QMessageBox::Yes)
        return;

    const QVector<Track> missing = m_db->findMissingTracks();
    for (const Track& t : missing)
        m_db->deleteTrack(t.id);

    reload();
    emit libraryChanged();
}
