#include "mainwindow.h"
#include "src/widgets/splashscreenwidget.h"

#include <QApplication>
#include <QTimer>

int main(int argc, char *argv[])
{
    QApplication application(argc, argv);
    application.setApplicationName(QStringLiteral("AURIX MediRun Dashboard"));

    auto *splashScreen = new SplashScreenWidget();
    auto *mainWindow = new MainWindow();
    QObject::connect(splashScreen, &SplashScreenWidget::finished, mainWindow, [mainWindow, splashScreen]() {
        mainWindow->showMaximized();
        splashScreen->deleteLater();
    });

    splashScreen->show();
    application.processEvents();

    QTimer::singleShot(SplashScreenWidget::kVisibleDurationMs, splashScreen, [splashScreen]() {
        splashScreen->notifyReady();
    });

    return QApplication::exec();
}
