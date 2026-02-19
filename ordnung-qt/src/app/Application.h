#pragma once

#include <QApplication>
#include <QString>

// Application — QApplication subclass.
// Handles: font setup, high-DPI attributes, app metadata, QSS loading.
// MainWindow applies the theme sheet to itself (scoped, not global).
class Application : public QApplication
{
    Q_OBJECT
public:
    Application(int& argc, char** argv);

    QString themeSheet() const { return m_themeSheet; }

private:
    void setupFont();
    void loadTheme();

    QString m_themeSheet;
};
