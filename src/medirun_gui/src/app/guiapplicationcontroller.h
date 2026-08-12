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
class CameraFeedProvider;
class OperationsOverviewService;
class RobotService;
class TelemetryLogRecorder;
class NavGoalLogRecorder;
class WaypointRepository;
class MissionStorageService;
class MissionRosInterface;
class MissionController;
class SensorTelemetryProvider;
class DrivingRecordService;

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
    std::unique_ptr<OperationsOverviewService> m_operationsOverviewService;
    std::unique_ptr<RobotService> m_robotService;
    std::unique_ptr<NavigationService> m_navigationService;
    std::unique_ptr<WaypointRepository> m_waypointRepository;
    std::unique_ptr<MissionStorageService> m_missionStorageService;
    std::unique_ptr<MissionRosInterface> m_missionRosInterface;
    std::unique_ptr<MissionController> m_missionController;
    TelemetryProvider *m_telemetryProvider;
    TelemetryLogRecorder *m_telemetryLogRecorder;
    NavGoalLogRecorder *m_navGoalLogRecorder;
    CameraFeedProvider *m_cameraFeedProvider;
    SensorTelemetryProvider *m_sensorTelemetryProvider {nullptr};
    DrivingRecordService *m_drivingRecordService {nullptr};
    SplashScreenWidget *m_splashScreen;
    LoginDialog *m_loginDialog;
    MainWindow *m_mainWindow;
    AppSession m_session;
};

#endif // GUIAPPLICATIONCONTROLLER_H
