#ifndef GUIAPPLICATIONCONTROLLER_H
#define GUIAPPLICATIONCONTROLLER_H

#include "appsession.h"
#include "../models/waypointtypes.h"

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
    // 대시보드를 처음 켰을 때 웨이포인트 저장소를 call_button DB 내용 +
    // HOME(waypoint_set.name = "home")만으로 초기화한다(그 외 파일/DB
    // 라이브러리 자동 로드 없음). 웨이포인트 파일이나 waypoint_set이 재부팅
    // 후 구버전으로 남아있어도, 사용자가 명시적으로 "불러오기"를 누르기
    // 전까지는 실제 등록된 BTN_* 좌표 + HOME만 신뢰하도록 한다.
    void resetWaypointRepositoryToStartupDefaults();
    // waypoint_set에 name="home"으로 저장된 라이브러리에서 kind=Home인
    // 항목만 골라 waypoints에 얹는다. 등록된 로봇이 정확히 1대가 아니거나
    // "home" 이름으로 저장된 게 없으면 조용히 건너뛴다.
    void mergeHomeWaypointFromDb(WaypointMap &waypoints) const;
    void connectWindowFlow();
    void presentMainWindow();
    void handleLoginAccepted();
    void handleLoginRejected();
    void handleLogout();
    // "대시보드 종료"/창 X 버튼으로 닫을 때는 MainWindow::close()가 창을
    // 숨기기만 할 뿐 실제로 delete하지 않는다(WA_DeleteOnClose 없음,
    // closeEvent 오버라이드 없음) — 그래서 로그아웃 경로에서만 실행되는
    // handleLogout()의 delete 로직을 이 경로에선 절대 안 타서, MainWindow에
    // 자식으로 붙은 ROS 프로바이더(RosBatteryTelemetryProvider 등)의
    // 소멸자(스핀 스레드 cancel()+join())가 한 번도 안 불리고 그대로
    // 프로세스가 끝나버렸다. QApplication::aboutToQuit는 종료 경로(버튼/
    // X버튼/시그널 무엇이든)와 무관하게 항상 발생하므로 여기서 한 번에
    // 정리한다. deleteLater()가 아니라 즉시 delete인 이유: aboutToQuit
    // 이후엔 이벤트 루프가 곧바로 멈춰서 deleteLater()가 예약한 삭제를
    // 처리할 기회가 없다.
    void handleAboutToQuit();

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
