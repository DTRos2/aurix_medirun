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
class OperationsOverviewService;
class RobotService;
class MissionController;
class WaypointRepository;
class SensorTelemetryProvider;
class DrivingRecordService;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(
        const AppSession &session,
        AuthService &authService,
        OperationsOverviewService &operationsOverviewService,
        RobotService &robotService,
        QWidget *parent = nullptr);
    ~MainWindow() override;

    void setTelemetryProvider(TelemetryProvider *provider);
    void setNavigationService(NavigationService *service);
    void setCameraFeedProvider(CameraFeedProvider *provider);
    void setMissionController(MissionController *controller);
    void setWaypointRepository(WaypointRepository *repository);
    void setSensorTelemetryProvider(SensorTelemetryProvider *provider);
    void setDrivingRecordService(DrivingRecordService *service);

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
    void handleNextQueueConfirmationRequested(const QString &queueId, const QString &waypointName);

private:
    Ui::MainWindow *ui;
    DashboardCentralWidget *m_dashboardWidget;
    AuthService &m_authService;
    OperationsOverviewService &m_operationsOverviewService;
    RobotService &m_robotService;
    AppSession m_session;
    MissionController *m_missionController {nullptr};
    NavigationService *m_navigationService {nullptr};
};
#endif // MAINWINDOW_H
