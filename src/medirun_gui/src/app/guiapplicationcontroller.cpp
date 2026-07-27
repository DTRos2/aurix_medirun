#include "guiapplicationcontroller.h"

#include "../../mainwindow.h"
#include "../dialogs/logindialog.h"
#include "../services/authservice.h"
#include "../services/localauthservice.h"
#include "../services/mocktelemetryprovider.h"
#include "../widgets/splashscreenwidget.h"

#include <QApplication>
#include <QDialog>
#include <QTimer>

GuiApplicationController::GuiApplicationController(QApplication &application, QObject *parent)
    : QObject(parent)
    , m_application(application)
    , m_authService(std::make_unique<LocalAuthService>())
    , m_telemetryProvider(nullptr)
    , m_splashScreen(nullptr)
    , m_loginDialog(nullptr)
    , m_mainWindow(nullptr)
{
}

GuiApplicationController::~GuiApplicationController() = default;

void GuiApplicationController::run()
{
    showSplashScreen();
    connectWindowFlow();
}

bool GuiApplicationController::mockModeEnabled() const
{
    return m_application.arguments().contains(QStringLiteral("--mock"));
}

void GuiApplicationController::showSplashScreen()
{
    m_splashScreen = new SplashScreenWidget();
    m_splashScreen->show();
    m_application.processEvents();
}

void GuiApplicationController::showLoginPage()
{
    if (m_loginDialog != nullptr) {
        m_loginDialog->raise();
        m_loginDialog->activateWindow();
        return;
    }

    m_loginDialog = new LoginDialog(*m_authService, mockModeEnabled(), nullptr);
    m_loginDialog->setAttribute(Qt::WA_DeleteOnClose, false);
    m_loginDialog->setModal(false);
    m_loginDialog->setWindowModality(Qt::NonModal);

    QObject::connect(m_loginDialog, &QDialog::accepted, this, &GuiApplicationController::handleLoginAccepted);
    QObject::connect(m_loginDialog, &QDialog::rejected, this, &GuiApplicationController::handleLoginRejected);
    m_loginDialog->show();
    m_loginDialog->raise();
    m_loginDialog->activateWindow();
}

void GuiApplicationController::createMainWindow()
{
    m_mainWindow = new MainWindow(m_session, *m_authService);
    m_telemetryProvider = new MockTelemetryProvider(m_mainWindow);
    m_mainWindow->setTelemetryProvider(m_telemetryProvider);
    QObject::connect(m_mainWindow, &MainWindow::logoutRequested, this, &GuiApplicationController::handleLogout);
}

void GuiApplicationController::presentMainWindow()
{
    createMainWindow();
    m_mainWindow->showMaximized();
    if (m_telemetryProvider != nullptr) {
        m_telemetryProvider->start();
    }
}

void GuiApplicationController::handleLoginAccepted()
{
    if (m_loginDialog == nullptr) {
        return;
    }

    m_session = m_loginDialog->session();
    m_loginDialog->hide();
    m_loginDialog->deleteLater();
    m_loginDialog = nullptr;
    presentMainWindow();
}

void GuiApplicationController::handleLoginRejected()
{
    if (m_loginDialog != nullptr) {
        m_loginDialog->deleteLater();
        m_loginDialog = nullptr;
    }

    QTimer::singleShot(0, &m_application, &QApplication::quit);
}

void GuiApplicationController::handleLogout()
{
    if (m_mainWindow != nullptr) {
        m_mainWindow->deleteLater();
        m_mainWindow = nullptr;
    }

    m_telemetryProvider = nullptr;
    showLoginPage();
}

void GuiApplicationController::connectWindowFlow()
{
    QObject::connect(m_splashScreen, &SplashScreenWidget::finished, this, [this]() {
        showLoginPage();
        m_splashScreen->deleteLater();
        m_splashScreen = nullptr;
    });

    QTimer::singleShot(SplashScreenWidget::kVisibleDurationMs, m_splashScreen, [this]() {
        m_splashScreen->notifyReady();
    });
}
