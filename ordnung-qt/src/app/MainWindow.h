#pragma once

#include <QMainWindow>
#include "core/ServiceRegistry.h"

class ConversionWorker;
class FolderWatcher;
class TrackModel;
class QUndoStack;
class QThread;
class QStackedWidget;
class QPushButton;
class LibraryView;

// MainWindow — sound file manager: Library tab only. Conversion and watch
// services remain in code for when the Downloads workflow is revisited.
class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(const QString& themeSheet, QWidget* parent = nullptr);
    ~MainWindow() override;

private slots:
    void onLibraryFolderChanged(const QString& path);
    void switchToLibrary();

private:
    void setupServices();
    void buildSidebar(QWidget* container);
    void restoreState();
    void setNavActive(QPushButton* active);

    // Service registry (DI container)
    ServiceRegistry*  m_registry      = nullptr;

    // Services
    class Database*   m_db = nullptr;
    ConversionWorker* m_converter    = nullptr;
    FolderWatcher*    m_watcher      = nullptr;
    QThread*          m_workerThread = nullptr;

    // Models
    TrackModel*     m_tracks    = nullptr;
    QUndoStack*     m_undoStack = nullptr;

    // UI (single content: Library)
    QPushButton*     m_libNavBtn   = nullptr;
    QStackedWidget* m_stack       = nullptr;
    LibraryView*     m_libraryView = nullptr;
};
