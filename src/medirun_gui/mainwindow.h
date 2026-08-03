#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "src/app/appsession.h"

#include <QMainWindow>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class QKeyEvent;
class AuthService;
class DashboardCentralWidget;
class TelemetryProvider;
class NavigationService;
class CameraFeedProvider;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(const AppSession &session, AuthService &authService, QWidget *parent = nullptr);
    ~MainWindow() override;

    void setTelemetryProvider(TelemetryProvider *provider);
    void setNavigationService(NavigationService *service);
    void setCameraFeedProvider(CameraFeedProvider *provider);

signals:
    void logoutRequested();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;

private slots:
    void handleDashboardExit();
    void handleEmergencyStop();
    void handleMissionCancel();
    void handleReturnHome();
    void handleLogout();

private:
    Ui::MainWindow *ui;
    DashboardCentralWidget *m_dashboardWidget;
    AuthService &m_authService;
    AppSession m_session;
};
#endif // MAINWINDOW_H
