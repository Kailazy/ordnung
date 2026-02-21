#include "DuplicateDetectorDialog.h"
#include "services/Database.h"
#include "style/Theme.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QMessageBox>
#include <QDebug>

// ── Helpers ───────────────────────────────────────────────────────────────────

QString DuplicateDetectorDialog::formatTrackCell(const Track& t)
{
    const QString artist = QString::fromStdString(t.artist);
    const QString title  = QString::fromStdString(t.title);
    const QString format = QString::fromStdString(t.format).toUpper();
    const QString bpm    = t.bpm > 0 ? QString::number(static_cast<int>(t.bpm)) : QString();

    QString result = artist.isEmpty() ? title : artist + QStringLiteral(" — ") + title;
    QStringList meta;
    if (!format.isEmpty()) meta << format;
    if (!bpm.isEmpty())    meta << bpm + QStringLiteral(" BPM");
    if (!meta.isEmpty())
        result += QStringLiteral("\n") + meta.join(QStringLiteral("  ·  "));
    return result;
}

// ── DuplicateDetectorDialog ───────────────────────────────────────────────────

DuplicateDetectorDialog::DuplicateDetectorDialog(Database* db, QWidget* parent)
    : QDialog(parent, Qt::Dialog | Qt::FramelessWindowHint)
    , m_db(db)
{
    setAttribute(Qt::WA_StyledBackground, true);
    setObjectName(QStringLiteral("duplicateDialog"));
    setMinimumWidth(760);
    setMinimumHeight(480);

    // ── Header ────────────────────────────────────────────────────────────────
    auto* header = new QWidget(this);
    header->setObjectName(QStringLiteral("dupHeader"));

    auto* titleLbl = new QLabel(QStringLiteral("DUPLICATE TRACKS"), header);
    titleLbl->setObjectName(QStringLiteral("dupTitle"));

    m_countLbl = new QLabel(header);
    m_countLbl->setObjectName(QStringLiteral("dupCount"));

    auto* dot = new QLabel(header);
    dot->setObjectName(QStringLiteral("batchEditDot"));
    dot->setFixedSize(6, 6);

    auto* hdrLayout = new QHBoxLayout(header);
    hdrLayout->setContentsMargins(
        Theme::Layout::PanelPad, Theme::Layout::ContentPadV,
        Theme::Layout::PanelPad, Theme::Layout::ContentPadV);
    hdrLayout->setSpacing(Theme::Layout::GapSm);
    hdrLayout->addWidget(titleLbl);
    hdrLayout->addWidget(m_countLbl);
    hdrLayout->addStretch();
    hdrLayout->addWidget(dot);

    // ── Table ─────────────────────────────────────────────────────────────────
    m_table = new QTableWidget(this);
    m_table->setColumnCount(4);
    m_table->setHorizontalHeaderLabels({
        QStringLiteral("Track A"),
        QStringLiteral("Track B"),
        QStringLiteral("Remove A"),
        QStringLiteral("Remove B")
    });
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Fixed);
    m_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Fixed);
    m_table->setColumnWidth(2, 90);
    m_table->setColumnWidth(3, 90);
    m_table->verticalHeader()->hide();
    m_table->setSelectionMode(QAbstractItemView::NoSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setAlternatingRowColors(false);
    m_table->setShowGrid(true);
    m_table->setObjectName(QStringLiteral("dupTable"));

    // ── Footer ────────────────────────────────────────────────────────────────
    auto* footer = new QWidget(this);
    footer->setObjectName(QStringLiteral("dupFooter"));

    auto* hint = new QLabel(
        QStringLiteral("Click 'Remove' to delete a track from the database. This cannot be undone."),
        footer);
    hint->setObjectName(QStringLiteral("batchEditHint"));

    m_closeBtn = new QPushButton(QStringLiteral("Close"), footer);
    m_closeBtn->setObjectName(QStringLiteral("missingCloseBtn"));
    connect(m_closeBtn, &QPushButton::clicked, this, &QDialog::accept);

    auto* footerLayout = new QHBoxLayout(footer);
    footerLayout->setContentsMargins(
        Theme::Layout::PanelPad, Theme::Layout::ContentPadV,
        Theme::Layout::PanelPad, Theme::Layout::ContentPadV);
    footerLayout->addWidget(hint);
    footerLayout->addStretch();
    footerLayout->addWidget(m_closeBtn);

    // ── Root layout ───────────────────────────────────────────────────────────
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);
    root->addWidget(header);
    root->addWidget(m_table, 1);
    root->addWidget(footer);

    // ── Scan for duplicates ────────────────────────────────────────────────────
    m_pairs = m_db->findDuplicateTracks();
    buildTable(m_pairs);
}

void DuplicateDetectorDialog::buildTable(const QVector<Database::DuplicatePair>& pairs)
{
    m_table->setRowCount(pairs.size());

    const int remaining = static_cast<int>(
        std::count_if(pairs.begin(), pairs.end(),
            [this](const Database::DuplicatePair& p) {
                return !m_removedIds.contains(p.a.id) && !m_removedIds.contains(p.b.id);
            }));
    m_countLbl->setText(QStringLiteral("%1 pair%2")
                            .arg(remaining)
                            .arg(remaining == 1 ? QString() : QStringLiteral("s")));

    for (int row = 0; row < pairs.size(); ++row) {
        const Database::DuplicatePair& pair = pairs[row];

        auto* itemA = new QTableWidgetItem(formatTrackCell(pair.a));
        auto* itemB = new QTableWidgetItem(formatTrackCell(pair.b));
        itemA->setTextAlignment(Qt::AlignTop | Qt::AlignLeft);
        itemB->setTextAlignment(Qt::AlignTop | Qt::AlignLeft);

        if (m_removedIds.contains(pair.a.id))
            itemA->setForeground(QColor(Theme::Color::Text3));
        if (m_removedIds.contains(pair.b.id))
            itemB->setForeground(QColor(Theme::Color::Text3));

        m_table->setItem(row, 0, itemA);
        m_table->setItem(row, 1, itemB);

        // Remove A button
        auto* btnA = new QPushButton(QStringLiteral("Remove"), m_table);
        btnA->setObjectName(QStringLiteral("missingRemoveBtn"));
        btnA->setEnabled(!m_removedIds.contains(pair.a.id) && !m_removedIds.contains(pair.b.id));
        connect(btnA, &QPushButton::clicked, this, [this, row]{ onRemoveA(row); });
        m_table->setCellWidget(row, 2, btnA);

        // Remove B button
        auto* btnB = new QPushButton(QStringLiteral("Remove"), m_table);
        btnB->setObjectName(QStringLiteral("missingRemoveBtn"));
        btnB->setEnabled(!m_removedIds.contains(pair.a.id) && !m_removedIds.contains(pair.b.id));
        connect(btnB, &QPushButton::clicked, this, [this, row]{ onRemoveB(row); });
        m_table->setCellWidget(row, 3, btnB);

        m_table->setRowHeight(row, 64);
    }
}

void DuplicateDetectorDialog::onRemoveA(int row)
{
    if (row < 0 || row >= m_pairs.size()) return;
    removeRow(row, m_pairs[row].a.id);
}

void DuplicateDetectorDialog::onRemoveB(int row)
{
    if (row < 0 || row >= m_pairs.size()) return;
    removeRow(row, m_pairs[row].b.id);
}

void DuplicateDetectorDialog::removeRow(int row, long long removeId)
{
    const QString title = removeId == m_pairs[row].a.id
        ? QString::fromStdString(m_pairs[row].a.title)
        : QString::fromStdString(m_pairs[row].b.title);

    const auto reply = QMessageBox::question(
        this, QStringLiteral("Remove Track"),
        QStringLiteral("Remove \"%1\" from the database?").arg(title),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);

    if (reply != QMessageBox::Yes) return;

    if (!m_db->deleteTrack(removeId)) {
        QMessageBox::warning(this, QStringLiteral("Error"),
            QStringLiteral("Failed to remove track: ") + m_db->errorString());
        return;
    }

    m_removedIds.append(removeId);
    qInfo() << "DuplicateDetectorDialog: removed track id=" << removeId;

    // Rebuild the table to reflect the removal
    buildTable(m_pairs);
}
