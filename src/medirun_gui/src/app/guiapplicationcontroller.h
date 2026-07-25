#ifndef GUIAPPLICATIONCONTROLLER_H
#define GUIAPPLICATIONCONTROLLER_H

#include <QObject>

class QApplication;
class MainWindow;
class SplashScreenWidget;

class GuiApplicationController : public QObject
{
    Q_OBJECT

public:
    explicit GuiApplicationController(QApplication &application, QObject *parent = nullptr);

    void run();

private:
    void showSplashScreen();
    void createMainWindow();
    void connectWindowFlow();

    QApplication &m_application;
    SplashScreenWidget *m_splashScreen;
    MainWindow *m_mainWindow;
};

#endif // GUIAPPLICATIONCONTROLLER_H
