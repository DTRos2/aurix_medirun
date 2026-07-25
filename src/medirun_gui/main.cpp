#include "mainwindow.h"
#include "src/app/guiapplicationcontroller.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication application(argc, argv);
    application.setApplicationName(QStringLiteral("AURIX MediRun Dashboard"));

    GuiApplicationController controller(application);
    controller.run();

    return QApplication::exec();
}
