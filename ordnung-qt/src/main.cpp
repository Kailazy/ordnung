#include "app/Application.h"
#include "app/MainWindow.h"

int main(int argc, char* argv[])
{
    Application app(argc, argv);

    MainWindow window(app.themeSheet());
    window.show();

    return app.exec();
}
