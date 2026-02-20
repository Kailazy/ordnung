#include "app/Application.h"

#include <QFont>
#include <QFile>
#include <QFontDatabase>

Application::Application(int& argc, char** argv)
    : QApplication(argc, argv)
{
    setOrganizationName("eyebags");
    setOrganizationDomain("eyebags.terminal");
    setApplicationName("eyebags-terminal");
    setApplicationVersion("1.0.0");

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    setAttribute(Qt::AA_EnableHighDpiScaling);
    setAttribute(Qt::AA_UseHighDpiPixmaps);
#endif

    setupFont();
    loadTheme();
}

void Application::setupFont()
{
    QFontDatabase::addApplicationFont(":/fonts/Figtree-Variable.ttf");

    QFont f("Figtree", 14);
    f.setWeight(QFont::Normal);       // 400
    f.setHintingPreference(QFont::PreferFullHinting);
    setFont(f);
}

void Application::loadTheme()
{
    QFile qss(":/theme.qss");
    if (qss.open(QIODevice::ReadOnly))
        m_themeSheet = QString::fromUtf8(qss.readAll());
}
