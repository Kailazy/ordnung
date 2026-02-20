#include "app/MainWindow.h"
#include "style/Theme.h"
#include <QDebug>

#include "views/LibraryView.h"
#include "models/TrackModel.h"
#include "services/Database.h"
#include "services/Converter.h"
#include "services/FolderWatcher.h"

#include <QWidget>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QStackedWidget>
#include <QPushButton>
#include <QLabel>
#include <QUndoStack>
#include <QThread>
#include <QMessageBox>
#include <QStyle>

MainWindow::MainWindow(const QString& themeSheet, QWidget* parent)
    : QMainWindow(parent)
{
    setMinimumSize(1100, 700);
    setWindowTitle("eyebags terminal");

    if (!themeSheet.isEmpty())
        setStyleSheet(themeSheet);

    setupServices();

    // ── Central widget ───────────────────────────────────────────────────────
    auto* central = new QWidget(this);
    setCentralWidget(central);

    auto* rootLayout = new QHBoxLayout(central);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    // ── Sidebar ──────────────────────────────────────────────────────────────
    auto* sidebar = new QWidget(central);
    sidebar->setObjectName("sidebar");
    sidebar->setFixedWidth(Theme::Layout::SidebarW);
    buildSidebar(sidebar);
    rootLayout->addWidget(sidebar);

    // ── Content stack (Library only; Downloads tab removed, revisit later) ───
    m_stack = new QStackedWidget(central);
    m_libraryView = new LibraryView(m_tracks, m_db, m_undoStack, m_stack);
    m_stack->addWidget(m_libraryView);

    rootLayout->addWidget(m_stack, 1);

    connect(m_libraryView, &LibraryView::libraryFolderChanged,
            this,          &MainWindow::onLibraryFolderChanged);

    restoreState();
}

MainWindow::~MainWindow()
{
    m_workerThread->quit();
    m_workerThread->wait();
}

void MainWindow::setupServices()
{
    m_db = new Database(this);
    if (!m_db->open()) {
        QMessageBox::critical(nullptr, "Database error",
            "Could not open database:\n" + m_db->errorString());
    }

    m_tracks    = new TrackModel(m_db, this);
    m_undoStack = new QUndoStack(this);

    // Conversion and watch services kept for when Downloads workflow is revisited.
    m_converter    = new ConversionWorker(m_db);
    m_workerThread = new QThread(this);
    m_converter->moveToThread(m_workerThread);
    m_workerThread->start();

    m_watcher = new FolderWatcher(m_db, this);

    // Register all services in the registry for view-level DI.
    m_registry = new ServiceRegistry();
    m_registry->registerService(m_db);
    m_registry->registerService(m_tracks);
    m_registry->registerService(m_undoStack);
    m_registry->registerService(m_converter);
}

void MainWindow::buildSidebar(QWidget* sidebar)
{
    auto* logo = new QLabel("eyebags\nterminal", sidebar);
    logo->setObjectName("logoLabel");
    logo->setTextInteractionFlags(Qt::NoTextInteraction);
    logo->setAlignment(Qt::AlignLeft | Qt::AlignBottom);
    {
        QFont f = logo->font();
        f.setPointSize(20);
        f.setWeight(QFont::Light);
        f.setLetterSpacing(QFont::AbsoluteSpacing, 3.0);
        logo->setFont(f);
    }

    m_libNavBtn = new QPushButton("library", sidebar);
    m_libNavBtn->setObjectName("navBtn");
    m_libNavBtn->setFocusPolicy(Qt::NoFocus);
    connect(m_libNavBtn, &QPushButton::clicked, this, &MainWindow::switchToLibrary);

    auto* version = new QLabel("v1.0.0", sidebar);
    version->setObjectName("versionLabel");
    version->setTextInteractionFlags(Qt::NoTextInteraction);

    auto* layout = new QVBoxLayout(sidebar);
    layout->setContentsMargins(0, Theme::Layout::Pad, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(logo);
    layout->addWidget(m_libNavBtn);
    layout->addStretch();
    layout->addWidget(version);

    setNavActive(m_libNavBtn);
}

void MainWindow::restoreState()
{
    const QString libFolder = m_db->loadLibraryFolder();
    qInfo() << "[MainWindow] Restoring library folder:" << (libFolder.isEmpty() ? "(none)" : libFolder);
    m_libraryView->setLibraryFolder(libFolder);
}

void MainWindow::setNavActive(QPushButton* active)
{
    if (m_libNavBtn) {
        m_libNavBtn->setProperty("active", m_libNavBtn == active);
        m_libNavBtn->style()->unpolish(m_libNavBtn);
        m_libNavBtn->style()->polish(m_libNavBtn);
    }
}

void MainWindow::switchToLibrary()
{
    m_stack->setCurrentIndex(0);
    setNavActive(m_libNavBtn);
}

void MainWindow::onLibraryFolderChanged(const QString& path)
{
    m_db->saveLibraryFolder(path);
}
