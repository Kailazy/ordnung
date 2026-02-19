#include "views/DownloadsView.h"

#include "models/DownloadsModel.h"
#include "delegates/StatusDelegate.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QPlainTextEdit>
#include <QTableView>
#include <QHeaderView>
#include <QScrollBar>
#include <QFileDialog>
#include <QMenu>
#include <QTimer>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QTextCursor>
#include <QTextCharFormat>
#include <QTextBlockFormat>
#include <QApplication>
#include <QFontMetrics>
#include <QSplitter>
#include <QFrame>
#include <QSvgRenderer>
#include <QPixmap>
#include <QPainter>

// ── FolderNode ────────────────────────────────────────────────────────────────

FolderNode::FolderNode(const QString& objectId, const QString& roleLabel, QWidget* parent)
    : QWidget(parent)
{
    setObjectName(objectId);
    setAttribute(Qt::WA_StyledBackground, true);
    setAttribute(Qt::WA_Hover, true);
    setCursor(Qt::PointingHandCursor);
    setMinimumWidth(190);
    setMaximumWidth(280);

    auto* layout = new QVBoxLayout(this);
    layout->setAlignment(Qt::AlignCenter);
    layout->setSpacing(10);
    layout->setContentsMargins(32, 26, 32, 22);

    auto* roleLbl = new QLabel(roleLabel.toUpper(), this);
    roleLbl->setObjectName("folderNodeLabel");
    roleLbl->setTextInteractionFlags(Qt::NoTextInteraction);
    roleLbl->setAlignment(Qt::AlignCenter);
    {
        QFont f = roleLbl->font();
        f.setPointSize(13);
        f.setLetterSpacing(QFont::AbsoluteSpacing, 2.0);
        roleLbl->setFont(f);
    }

    m_pathLabel = new QLabel("not set", this);
    m_pathLabel->setObjectName("folderNodePath");
    m_pathLabel->setTextInteractionFlags(Qt::NoTextInteraction);
    m_pathLabel->setAlignment(Qt::AlignCenter);

    layout->addWidget(roleLbl);
    layout->addWidget(m_pathLabel);
}

void FolderNode::setPath(const QString& path)
{
    m_path = path;
    updatePathLabel();
}

void FolderNode::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton)
        emit clicked();
    QWidget::mousePressEvent(event);
}

void FolderNode::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    updatePathLabel();
}

void FolderNode::updatePathLabel()
{
    const QString display = m_path.isEmpty() ? "not set" : m_path;
    const QFontMetrics fm(m_pathLabel->font());
    const int avail = width() - 64;  // 32px padding each side
    m_pathLabel->setText(fm.elidedText(display, Qt::ElideMiddle, qMax(avail, 40)));
}

// ── DownloadsView ─────────────────────────────────────────────────────────────

DownloadsView::DownloadsView(DownloadsModel* model, QWidget* parent)
    : QWidget(parent)
    , m_model(model)
    , m_delegate(new StatusDelegate(this))
{
    // ── Folder flow ──────────────────────────────────────────────────────────
    m_srcNode = new FolderNode("folderNodeSrc", "source", this);
    m_outNode = new FolderNode("folderNodeOut", "aiff output", this);

    static const char arrowSvg[] =
        "<svg viewBox='0 0 48 24' fill='none' stroke='#444444' stroke-width='2' "
        "stroke-linecap='round' stroke-linejoin='round'>"
        "<line x1='2' y1='12' x2='38' y2='12'/>"
        "<polyline points='32,6 38,12 32,18'/>"
        "</svg>";

    QPixmap arrowPx(64, 28);
    arrowPx.fill(Qt::transparent);
    {
        QByteArray svgBytes(arrowSvg);
        QSvgRenderer svgR(svgBytes);
        QPainter svgP(&arrowPx);
        svgR.render(&svgP);
    }
    auto* arrowLabel = new QLabel(this);
    arrowLabel->setPixmap(arrowPx);
    arrowLabel->setContentsMargins(10, 0, 10, 0);

    auto* folderFlow = new QWidget(this);
    folderFlow->setObjectName("folderFlow");
    auto* flowLayout = new QHBoxLayout(folderFlow);
    flowLayout->setContentsMargins(0, 24, 0, 12);
    flowLayout->setAlignment(Qt::AlignHCenter);
    flowLayout->setSpacing(0);
    flowLayout->addWidget(m_srcNode);
    flowLayout->addWidget(arrowLabel);
    flowLayout->addWidget(m_outNode);

    // ── Action bar ───────────────────────────────────────────────────────────
    m_saveBtn       = new QPushButton("save",        this);
    m_scanBtn       = new QPushButton("scan",        this);
    m_convertAllBtn = new QPushButton("convert all", this);
    m_busyLabel     = new QLabel(this);

    m_convertAllBtn->setProperty("btnStyle", "accent");
    m_busyLabel->setObjectName("converterStatus");
    m_busyLabel->setTextInteractionFlags(Qt::NoTextInteraction);
    m_busyLabel->setVisible(false);

    auto* actionBar = new QWidget(this);
    auto* actionLayout = new QHBoxLayout(actionBar);
    actionLayout->setContentsMargins(0, 0, 0, 16);
    actionLayout->setAlignment(Qt::AlignHCenter);
    actionLayout->setSpacing(12);
    actionLayout->addWidget(m_saveBtn);
    actionLayout->addWidget(m_scanBtn);
    actionLayout->addWidget(m_convertAllBtn);
    actionLayout->addWidget(m_busyLabel);

    // ── Downloads table ──────────────────────────────────────────────────────
    m_tableView = new QTableView(this);
    m_tableView->setModel(model);
    m_tableView->setItemDelegate(m_delegate);
    m_tableView->setShowGrid(false);
    m_tableView->setAlternatingRowColors(false);
    m_tableView->setMouseTracking(true);
    m_tableView->viewport()->setMouseTracking(true);
    m_tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tableView->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tableView->setContextMenuPolicy(Qt::CustomContextMenu);
    m_tableView->verticalHeader()->hide();
    m_tableView->verticalHeader()->setDefaultSectionSize(46);
    m_tableView->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_tableView->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);

    auto* hdr = m_tableView->horizontalHeader();
    hdr->setSectionResizeMode(DownloadsModel::ColFilename, QHeaderView::Stretch);
    hdr->setSectionResizeMode(DownloadsModel::ColExt,      QHeaderView::Fixed);
    hdr->setSectionResizeMode(DownloadsModel::ColSize,     QHeaderView::Fixed);
    hdr->setSectionResizeMode(DownloadsModel::ColStatus,   QHeaderView::Fixed);
    hdr->setSectionResizeMode(DownloadsModel::ColAction,   QHeaderView::Fixed);
    m_tableView->setColumnWidth(DownloadsModel::ColExt,     80);
    m_tableView->setColumnWidth(DownloadsModel::ColSize,    80);
    m_tableView->setColumnWidth(DownloadsModel::ColStatus, 135);
    m_tableView->setColumnWidth(DownloadsModel::ColAction,  40);

    // ── Activity log ─────────────────────────────────────────────────────────
    auto* logHeader = new QWidget(this);
    logHeader->setObjectName("activityLogHeader");
    logHeader->setFixedHeight(32);

    auto* logTitle = new QLabel("activity log", logHeader);
    logTitle->setObjectName("activityLogTitle");
    logTitle->setTextInteractionFlags(Qt::NoTextInteraction);
    {
        QFont f = logTitle->font();
        f.setPointSize(13);
        f.setLetterSpacing(QFont::AbsoluteSpacing, 1.5);
        logTitle->setFont(f);
    }

    m_logDot = new QLabel(logHeader);
    m_logDot->setObjectName("logDot");
    m_logDot->setFixedSize(7, 7);

    auto* logHeaderLayout = new QHBoxLayout(logHeader);
    logHeaderLayout->setContentsMargins(28, 0, 28, 0);
    logHeaderLayout->setSpacing(9);
    logHeaderLayout->addWidget(logTitle);
    logHeaderLayout->addWidget(m_logDot);
    logHeaderLayout->addStretch();

    m_logEdit = new QPlainTextEdit(this);
    m_logEdit->setObjectName("activityLog");
    m_logEdit->setReadOnly(true);
    m_logEdit->setLineWrapMode(QPlainTextEdit::NoWrap);
    m_logEdit->setMinimumHeight(160);
    m_logEdit->setMaximumHeight(250);
    {
        QFont mono("Consolas");
        mono.setStyleHint(QFont::TypeWriter);
        mono.setPointSize(13);
        m_logEdit->setFont(mono);
    }
    // Apply default block line-height (1.7x) on the document
    {
        QTextCursor cur = m_logEdit->textCursor();
        QTextBlockFormat blkFmt;
        blkFmt.setLineHeight(170, QTextBlockFormat::ProportionalHeight);
        cur.setBlockFormat(blkFmt);
        m_logEdit->setTextCursor(cur);
    }

    m_logDotTimer = new QTimer(this);
    m_logDotTimer->setSingleShot(true);
    connect(m_logDotTimer, &QTimer::timeout, this, [this] {
        m_logDot->setStyleSheet("background-color: #444444; border-radius: 3px;");
    });

    auto* logSection = new QWidget(this);
    auto* logLayout  = new QVBoxLayout(logSection);
    logLayout->setContentsMargins(0, 0, 0, 0);
    logLayout->setSpacing(0);
    logLayout->addWidget(logHeader);
    logLayout->addWidget(m_logEdit);

    // ── Root layout ──────────────────────────────────────────────────────────
    auto* vSplitter = new QSplitter(Qt::Vertical, this);
    vSplitter->addWidget(m_tableView);
    vSplitter->addWidget(logSection);
    vSplitter->setCollapsible(0, false);
    vSplitter->setCollapsible(1, false);
    vSplitter->setHandleWidth(1);
    vSplitter->setSizes({400, 200});

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);
    root->addWidget(folderFlow);
    root->addWidget(actionBar);
    root->addWidget(vSplitter, 1);

    // ── Connections ──────────────────────────────────────────────────────────
    connect(m_srcNode, &FolderNode::clicked, this, &DownloadsView::onSrcNodeClicked);
    connect(m_outNode, &FolderNode::clicked, this, &DownloadsView::onOutNodeClicked);
    connect(m_saveBtn,       &QPushButton::clicked, this, &DownloadsView::onSaveClicked);
    connect(m_scanBtn,       &QPushButton::clicked, this, &DownloadsView::onScanClicked);
    connect(m_convertAllBtn, &QPushButton::clicked, this, &DownloadsView::onConvertAllClicked);
    connect(m_delegate, &StatusDelegate::convertRequested,
            this, &DownloadsView::onConvertRequested);
    connect(m_tableView, &QTableView::customContextMenuRequested,
            this, &DownloadsView::onTableContextMenu);
}

void DownloadsView::setWatchConfig(const WatchConfig& cfg)
{
    m_srcNode->setPath(cfg.watchFolder);
    m_outNode->setPath(cfg.outputFolder);
}

WatchConfig DownloadsView::currentConfig() const
{
    WatchConfig cfg;
    cfg.watchFolder  = m_srcNode->path();
    cfg.outputFolder = m_outNode->path();
    return cfg;
}

void DownloadsView::appendLogLine(const QString& line)
{
    QTextDocument* doc = m_logEdit->document();

    // Trim to 200 lines
    if (doc->blockCount() >= 200) {
        QTextCursor trim(doc);
        trim.movePosition(QTextCursor::Start);
        trim.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
        trim.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor);
        trim.removeSelectedText();
    }

    QTextCursor cursor(doc);
    cursor.movePosition(QTextCursor::End);

    QTextCharFormat fmt;
    if (line.contains("ERROR", Qt::CaseInsensitive))
        fmt.setForeground(QColor("#e57373"));
    else if (line.contains("WARN", Qt::CaseInsensitive))
        fmt.setForeground(QColor("#ffb74d"));
    else
        fmt.setForeground(QColor("#444444"));

    cursor.insertText(line + "\n", fmt);
    m_logEdit->verticalScrollBar()->setValue(m_logEdit->verticalScrollBar()->maximum());
    triggerLogPulse();
}

void DownloadsView::onConversionUpdate(long long downloadId, long long convId,
                                       ConversionStatus status, const QString& error)
{
    m_model->setConversionStatus(downloadId, convId, status, error);
}

void DownloadsView::reloadTable()
{
    m_model->reload();
}

void DownloadsView::onSrcNodeClicked()
{
    const QString dir = QFileDialog::getExistingDirectory(
        this, "Select Source Folder", m_srcNode->path());
    if (!dir.isEmpty())
        m_srcNode->setPath(dir);
}

void DownloadsView::onOutNodeClicked()
{
    const QString dir = QFileDialog::getExistingDirectory(
        this, "Select Output Folder", m_outNode->path());
    if (!dir.isEmpty())
        m_outNode->setPath(dir);
}

void DownloadsView::onSaveClicked()
{
    WatchConfig cfg;
    cfg.watchFolder  = m_srcNode->path();
    cfg.outputFolder = m_outNode->path();
    emit saveConfigRequested(cfg);
}

void DownloadsView::onScanClicked()
{
    const QString folder = m_srcNode->path();
    if (!folder.isEmpty())
        emit scanRequested(folder);
}

void DownloadsView::onConvertAllClicked()
{
    emit convertAllRequested(m_outNode->path());
}

void DownloadsView::onConvertRequested(const QModelIndex& index)
{
    const Download* dl = m_model->downloadAt(index.row());
    if (!dl) return;
    emit convertSingleRequested(dl->id,
                                QString::fromStdString(dl->filepath),
                                m_outNode->path());
}

void DownloadsView::onTableContextMenu(const QPoint& pos)
{
    const QModelIndex idx = m_tableView->indexAt(pos);
    if (!idx.isValid()) return;
    const Download* dl = m_model->downloadAt(idx.row());
    if (!dl) return;

    QMenu menu(this);
    auto* deleteAct = menu.addAction("Delete");
    if (menu.exec(m_tableView->viewport()->mapToGlobal(pos)) == deleteAct)
        emit deleteDownloadRequested(dl->id);
}

void DownloadsView::triggerLogPulse()
{
    m_logDot->setStyleSheet("background-color: #81c784; border-radius: 3px;");
    m_logDotTimer->start(1200);
}
