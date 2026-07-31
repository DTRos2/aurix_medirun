#ifndef GUIAPPLICATIONCONTROLLER_H
#define GUIAPPLICATIONCONTROLLER_H

#include "appsession.h"

#include <QObject>
#include <memory>

class QApplication;
class AuthService;
class LoginDialog;
class MainWindow;
class SplashScreenWidget;
class TelemetryProvider;
class NavigationService;

class GuiApplicationController : public QObject
{
    Q_OBJECT

public:
    explicit GuiApplicationController(QApplication &application, QObject *parent = nullptr);
    ~GuiApplicationController() override;

    void run();

private:
    bool mockModeEnabled() const;
    void showSplashScreen();
    void showLoginPage();
    void createMainWindow();
    void connectWindowFlow();
    void presentMainWindow();
    void handleLoginAccepted();
    void handleLoginRejected();
    void handleLogout();

    QApplication &m_application;
    std::unique_ptr<AuthService> m_authService;
    std::unique_ptr<NavigationService> m_navigationService;
    TelemetryProvider *m_telemetryProvider;
    SplashScreenWidget *m_splashScreen;
    LoginDialog *m_loginDialog;
    MainWindow *m_mainWindow;
    AppSession m_session;
};

#endif // GUIAPPLICATIONCONTROLLER_H
