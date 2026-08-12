#include "guiapplicationcontroller.h"

#include "../../mainwindow.h"
#include "../dialogs/logindialog.h"
#include "../ros/nav2navigationservice.h"
#include "../services/authservice.h"
#include "../services/dbauthservice.h"
#include "../services/dboperationsoverviewservice.h"
#include "../services/dbrobotservice.h"
#include "../services/drivingrecordservice.h"
#include "../services/missioncontroller.h"
#include "../services/missionrosinterface.h"
#include "../services/missionstorageservice.h"
#include "../services/mockcamerafeedprovider.h"
#include "../services/mocksensortelemetryprovider.h"
#include "../services/mocktelemetryprovider.h"
#include "../services/navgoallogrecorder.h"
#include "../services/roscamerafeedprovider.h"
#include "../services/rosbatterytelemetryprovider.h"
#include "../services/rossensortelemetryprovider.h"
#include "../services/telemetrylogrecorder.h"
#include "../services/waypointrepository.h"
#include "../widgets/splashscreenwidget.h"

#include <QApplication>
#include <QDebug>
#include <QDialog>
#include <QTimer>

GuiApplicationController::GuiApplicationController(QApplication &application, QObject *parent)
    : QObject(parent)
    , m_application(application)
    , m_authService(std::make_unique<DbAuthService>())
    , m_operationsOverviewService(std::make_unique<DbOperationsOverviewService>())
    , m_robotService(std::make_unique<DbRobotService>())
    , m_navigationService(std::make_unique<Nav2NavigationService>())
    , m_waypointRepository(std::make_unique<WaypointRepository>())
    , m_missionStorageService(std::make_unique<MissionStorageService>())
    , m_missionRosInterface(std::make_unique<MissionRosInterface>())
    , m_missionController(nullptr)
    , m_telemetryProvider(nullptr)
    , m_telemetryLogRecorder(nullptr)
    , m_navGoalLogRecorder(nullptr)
    , m_cameraFeedProvider(nullptr)
    , m_sensorTelemetryProvider(nullptr)
    , m_drivingRecordService(nullptr)
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

void GuiApplicationController::mergeHomeWaypointFromDb(WaypointMap &waypoints) const
{
    const QVector<RobotRecord> robots = m_robotService->listRobots(false);
    if (robots.size() != 1) {
        return;
    }

    QString frameId;
    WaypointMap homeWaypoints;
    TrajectoryMap trajectories;
    QString errorMessage;
    if (!m_operationsOverviewService->loadWaypointLibraryByName(
            robots.first().id, QStringLiteral("home"), frameId, homeWaypoints, trajectories, errorMessage)) {
        return;
    }

    for (auto it = homeWaypoints.cbegin(); it != homeWaypoints.cend(); ++it) {
        if (it.value().kind == WaypointMarkerKind::Home) {
            waypoints.insert(it.key(), it.value());
        }
    }
}

void GuiApplicationController::resetWaypointRepositoryToStartupDefaults()
{
    QVector<CallButtonRow> rows;
    QString errorMessage;
    if (!m_operationsOverviewService->loadActiveCallButtons(rows, errorMessage)) {
        qWarning() << "call_button 로드 실패, 초기 웨이포인트를 비워둡니다:" << errorMessage;
        rows.clear();
    }

    WaypointMap waypoints;
    for (const CallButtonRow &row : std::as_const(rows)) {
        WaypointDef def;
        def.name = row.code;
        def.label = row.zoneName.isEmpty() ? row.code : row.zoneName;
        def.x = row.x;
        def.y = row.y;
        def.yaw = row.yaw;
        def.kind = WaypointMarkerKind::CallZone;
        waypoints.insert(def.name, def);
    }
    mergeHomeWaypointFromDb(waypoints);

    // WaypointRepository 생성자가 이미 기본 웨이포인트 파일을 읽어둔 상태지만,
    // 대시보드 최초 진입 화면은 그 파일 내용을 무시하고 call_button + HOME만
    // 반영해야 하므로 통째로 교체한다. 사용자가 "웨이포인트 불러오기"를
    // 명시적으로 눌러야 그 파일 내용이 여기에 합쳐져서 다시 채워진다.
    m_waypointRepository->replaceAll(QStringLiteral("map"), waypoints, TrajectoryMap(), QString());
}

void GuiApplicationController::createMainWindow()
{
    m_mainWindow = new MainWindow(m_session, *m_authService, *m_operationsOverviewService, *m_robotService);
    resetWaypointRepositoryToStartupDefaults();
    m_missionController = std::make_unique<MissionController>(
        m_session,
        m_navigationService.get(),
        m_waypointRepository.get(),
        m_missionStorageService.get(),
        m_missionRosInterface.get(),
        nullptr);
    if (mockModeEnabled()) {
        m_telemetryProvider = new MockTelemetryProvider(m_mainWindow);
    } else {
        m_telemetryProvider = new RosBatteryTelemetryProvider(m_mainWindow);
    }
    m_mainWindow->setTelemetryProvider(m_telemetryProvider);
    m_mainWindow->setNavigationService(m_navigationService.get());

    m_telemetryLogRecorder = new TelemetryLogRecorder(*m_robotService, *m_operationsOverviewService, m_mainWindow);
    QObject::connect(
        m_telemetryProvider, &TelemetryProvider::snapshotUpdated,
        m_telemetryLogRecorder, &TelemetryLogRecorder::handleSnapshot,
        Qt::QueuedConnection);

    m_navGoalLogRecorder = new NavGoalLogRecorder(
        *m_robotService, *m_operationsOverviewService, m_session.username, m_session.userId, m_mainWindow);
    QObject::connect(
        m_navigationService.get(), &NavigationService::navSessionStarted,
        m_navGoalLogRecorder, &NavGoalLogRecorder::handleSessionStarted);
    QObject::connect(
        m_navigationService.get(), &NavigationService::waypointProgress,
        m_navGoalLogRecorder, &NavGoalLogRecorder::handleWaypointProgress);
    QObject::connect(
        m_navigationService.get(), &NavigationService::navSessionFinished,
        m_navGoalLogRecorder, &NavGoalLogRecorder::handleSessionFinished);
    QObject::connect(
        m_navigationService.get(), &NavigationService::amclPoseUpdated,
        m_navGoalLogRecorder, &NavGoalLogRecorder::handleAmclPose);

    if (mockModeEnabled()) {
        m_cameraFeedProvider = new MockCameraFeedProvider(m_mainWindow);
    } else {
        m_cameraFeedProvider = new RosCameraFeedProvider(m_mainWindow);
    }
    m_mainWindow->setCameraFeedProvider(m_cameraFeedProvider);
    m_mainWindow->setWaypointRepository(m_waypointRepository.get());
    m_mainWindow->setMissionController(m_missionController.get());

    if (mockModeEnabled()) {
        m_sensorTelemetryProvider = new MockSensorTelemetryProvider(m_mainWindow);
    } else {
        m_sensorTelemetryProvider = new RosSensorTelemetryProvider(m_mainWindow);
    }
    m_mainWindow->setSensorTelemetryProvider(m_sensorTelemetryProvider);

    m_drivingRecordService = new DrivingRecordService(
        m_session, m_navigationService.get(), m_telemetryProvider, m_cameraFeedProvider, m_waypointRepository.get(),
        *m_robotService, *m_operationsOverviewService, m_mainWindow);
    m_mainWindow->setDrivingRecordService(m_drivingRecordService);

    QObject::connect(m_mainWindow, &MainWindow::logoutRequested, this, &GuiApplicationController::handleLogout);
}

void GuiApplicationController::presentMainWindow()
{
    createMainWindow();
    m_mainWindow->showMaximized();
    if (m_telemetryProvider != nullptr) {
        m_telemetryProvider->start();
    }
    if (m_cameraFeedProvider != nullptr) {
        m_cameraFeedProvider->start();
    }
    if (m_sensorTelemetryProvider != nullptr) {
        m_sensorTelemetryProvider->start();
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
    m_telemetryLogRecorder = nullptr;
    m_navGoalLogRecorder = nullptr;
    m_cameraFeedProvider = nullptr;
    m_missionController.reset();
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
