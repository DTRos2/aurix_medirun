#include "guiapplicationcontroller.h"

#include "../../mainwindow.h"
#include "../widgets/splashscreenwidget.h"

#include <QApplication>
#include <QTimer>

GuiApplicationController::GuiApplicationController(QApplication &application, QObject *parent)
    : QObject(parent)
    , m_application(application)
    , m_splashScreen(nullptr)
    , m_mainWindow(nullptr)
{
}

void GuiApplicationController::run()
{
    createMainWindow();
    showSplashScreen();
    connectWindowFlow();
}

void GuiApplicationController::showSplashScreen()
{
    m_splashScreen = new SplashScreenWidget();
    m_splashScreen->show();
    m_application.processEvents();
}

void GuiApplicationController::createMainWindow()
{
    m_mainWindow = new MainWindow();
}

void GuiApplicationController::connectWindowFlow()
{
    QObject::connect(m_splashScreen, &SplashScreenWidget::finished, m_mainWindow, [this]() {
        m_mainWindow->showMaximized();
        m_splashScreen->deleteLater();
    });

    QTimer::singleShot(SplashScreenWidget::kVisibleDurationMs, m_splashScreen, [this]() {
        m_splashScreen->notifyReady();
    });
}

