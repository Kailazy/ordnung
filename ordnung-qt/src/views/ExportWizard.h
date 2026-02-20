#pragma once

#include <QDialog>

class Database;
class ExportService;
class QStackedWidget;
class QPushButton;
class QLineEdit;
class QLabel;
class QProgressBar;
class QScrollArea;
class QCheckBox;
class QButtonGroup;
struct ExportOptions;

// ExportWizard — two-step dialog for exporting playlists to Rekordbox XML
// or Pioneer CDJ USB format.
//
// Step 1: Choose export target (Rekordbox XML / CDJ USB) and output path.
// Step 2: Select playlists, output format, and file copy options.
// Footer: Back/Export buttons, 4px progress strip (shown during export).
class ExportWizard : public QDialog
{
    Q_OBJECT
public:
    explicit ExportWizard(Database* db, QWidget* parent = nullptr);

    // Pre-select a specific playlist (used from context menu).
    void preselectPlaylist(long long playlistId);

private slots:
    // Advance from step 1 to step 2, or start export on step 2.
    void onConfirmClicked();

    // Navigate back to step 1.
    void onBackClicked();

    // Browse for output path.
    void onBrowseClicked();

    // Handle export progress updates.
    void onExportProgress(int done, int total, const QString& currentFile);

    // Handle export completion.
    void onExportFinished(bool success, const QString& errorMsg);

private:
    QWidget* buildHeader();
    QWidget* buildPage1();
    QWidget* buildPage2();
    QWidget* buildFooter();
    void setActivePage(int page);
    void setExporting(bool exporting);
    void startExport();

    Database*       m_db;
    ExportService*  m_exportService = nullptr;

    // Header
    QLabel*         m_stepLabel         = nullptr;
    QLabel*         m_sectionTitle      = nullptr;

    // Pages
    QStackedWidget* m_pages             = nullptr;

    // Page 1: target selection
    QPushButton*    m_xmlCard           = nullptr;
    QPushButton*    m_usbCard           = nullptr;
    QLabel*         m_pathLabel         = nullptr;
    QLineEdit*      m_pathEdit          = nullptr;
    QPushButton*    m_browseBtn         = nullptr;

    // Page 2: options
    QScrollArea*    m_playlistScroll    = nullptr;
    QWidget*        m_playlistListWidget = nullptr;
    QButtonGroup*   m_formatGroup       = nullptr;
    QCheckBox*      m_copyFilesCheck    = nullptr;
    QLabel*         m_sizeEstimate      = nullptr;

    // Footer
    QPushButton*    m_backBtn           = nullptr;
    QPushButton*    m_confirmBtn        = nullptr;
    QProgressBar*   m_progressStrip     = nullptr;

    long long       m_preselectedPlaylistId = -1;
};
