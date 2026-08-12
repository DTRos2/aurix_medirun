#include "dashboardcentralwidget.h"

#include "headerbar.h"
#include "sidenav.h"

#include "../pages/callhistorypage.h"
#include "../pages/livecontrolpage.h"
#include "../pages/missionopspage.h"
#include "../pages/operationsoverviewpage.h"
#include "../pages/recordsplaybackpage.h"
#include "../pages/remotecontrolpage.h"
#include "../pages/robotmanagementpage.h"
#include "../pages/slammappingpage.h"
#include "../pages/systemsettingspage.h"
#include "../pages/usermanagementpage.h"
#include "../pages/waypointmanagementpage.h"
#include "../ros/navigationservice.h"
#include "../services/camerafeedprovider.h"
#include "../services/missioncontroller.h"
#include "../services/waypointrepository.h"
#include "../services/operationsoverviewservice.h"

#include <QHBoxLayout>
#include <QStackedWidget>
#include <QVBoxLayout>

DashboardCentralWidget::DashboardCentralWidget(
    const AppSession &session,
    AuthService &authService,
    OperationsOverviewService &operationsOverviewService,
    RobotService &robotService,
    QWidget *parent)
    : QWidget(parent)
    , m_authService(authService)
    , m_session(session)
    , m_headerBar(nullptr)
    , m_sideNav(nullptr)
    , m_stack(nullptr)
{
    setObjectName(QStringLiteral("dashboardRoot"));

    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(14, 14, 14, 14);
    rootLayout->setSpacing(12);

    m_headerBar = new HeaderBar(this);
    connect(m_headerBar, &HeaderBar::estopRequested, this, &DashboardCentralWidget::estopRequested);
    rootLayout->addWidget(m_headerBar);

    auto *bodyLayout = new QHBoxLayout;
    bodyLayout->setContentsMargins(0, 0, 0, 0);
    bodyLayout->setSpacing(12);

    m_sideNav = new SideNav(m_session, this);
    connect(m_sideNav, &SideNav::pageSelected, this, &DashboardCentralWidget::navigateTo);
    connect(m_sideNav, &SideNav::logoutRequested, this, &DashboardCentralWidget::logoutRequested);
    connect(m_sideNav, &SideNav::exitRequested, this, &DashboardCentralWidget::exitRequested);
    bodyLayout->addWidget(m_sideNav);

    m_stack = new QStackedWidget(this);
    bodyLayout->addWidget(m_stack, 1);

    rootLayout->addLayout(bodyLayout, 1);

    m_liveControlPage = new LiveControlPage(m_stack);
    m_liveControlPage->setSession(m_session);
    m_liveControlPage->setOperationsOverviewService(&operationsOverviewService);
    connect(m_liveControlPage, &LiveControlPage::missionCancelRequested, this, &DashboardCentralWidget::missionCancelRequested);
    connect(m_liveControlPage, &LiveControlPage::returnHomeRequested, this, &DashboardCentralWidget::returnHomeRequested);
    addPage(PageId::LiveControl, m_liveControlPage);

    m_missionOpsPage = new MissionOpsPage(m_stack);
    connect(m_missionOpsPage, &MissionOpsPage::missionCancelRequested, this, &DashboardCentralWidget::missionCancelRequested);
    connect(m_missionOpsPage, &MissionOpsPage::returnHomeRequested, this, &DashboardCentralWidget::returnHomeRequested);
    connect(m_missionOpsPage, &MissionOpsPage::remoteControlRequested, this, [this]() { navigateTo(PageId::RemoteControl); });
    addPage(PageId::MissionOps, m_missionOpsPage);

    addPage(PageId::RecordsPlayback, new RecordsPlaybackPage(m_stack));

    auto *callHistory = new CallHistoryPage(m_stack);
    connect(callHistory, &CallHistoryPage::replayRequested, this, [this]() { navigateTo(PageId::RecordsPlayback); });
    addPage(PageId::CallHistory, callHistory);

    m_operationsOverviewPage = new OperationsOverviewPage(operationsOverviewService, m_stack);
    addPage(PageId::OperationsOverview, m_operationsOverviewPage);
    m_remoteControlPage = new RemoteControlPage(m_stack);
    addPage(PageId::RemoteControl, m_remoteControlPage);

    if (m_session.role == UserRole::Admin) {
        m_slamMappingPage = new SlamMappingPage(m_stack);
        addPage(PageId::SlamMapping, m_slamMappingPage);
        m_waypointPage = new WaypointManagementPage(robotService, operationsOverviewService, m_stack);
        addPage(PageId::WaypointManagement, m_waypointPage);
        addPage(PageId::UserManagement, new UserManagementPage(m_authService, m_stack));
        addPage(PageId::RobotManagement, new RobotManagementPage(robotService, m_stack));
        addPage(PageId::SystemSettings, new SystemSettingsPage(operationsOverviewService, m_stack));
    }

    navigateTo(PageId::OperationsOverview);
}

void DashboardCentralWidget::applySnapshot(const TelemetrySnapshot &snapshot)
{
    m_headerBar->applySnapshot(snapshot);
    if (m_operationsOverviewPage != nullptr) {
        m_operationsOverviewPage->applyTelemetrySnapshot(snapshot);
    }
}

void DashboardCentralWidget::setNavigationService(NavigationService *service)
{
    if (m_waypointPage != nullptr) {
        m_waypointPage->setNavigationService(service);
    }
    if (m_remoteControlPage != nullptr) {
        m_remoteControlPage->setNavigationService(service);
    }
    if (m_liveControlPage != nullptr) {
        m_liveControlPage->setNavigationService(service);
    }
    if (service != nullptr && m_operationsOverviewPage != nullptr) {
        connect(service, &NavigationService::nav2ConnectionChanged, m_operationsOverviewPage, &OperationsOverviewPage::applyNav2Connection);
        connect(service, &NavigationService::amclPoseUpdated, m_operationsOverviewPage, &OperationsOverviewPage::applyAmclPose);
        connect(service, &NavigationService::statusChanged, m_operationsOverviewPage, &OperationsOverviewPage::applyNavStatus);
    }
}

void DashboardCentralWidget::setMissionController(MissionController *controller)
{
    if (m_missionOpsPage != nullptr) {
        m_missionOpsPage->setMissionController(controller);
        connect(m_missionOpsPage, &MissionOpsPage::retryRequested, controller, &MissionController::requestRetry);
        connect(m_missionOpsPage, &MissionOpsPage::queueCancelRequested, controller, &MissionController::requestQueueCancel);
        connect(m_missionOpsPage, &MissionOpsPage::queueDivertRequested, controller, &MissionController::requestQueueDivert);
    }
    if (m_liveControlPage != nullptr) {
        m_liveControlPage->setMissionController(controller);
    }
}

void DashboardCentralWidget::setWaypointRepository(WaypointRepository *repository)
{
    if (m_waypointPage != nullptr) {
        m_waypointPage->setWaypointRepository(repository);
    }
    if (m_liveControlPage != nullptr) {
        m_liveControlPage->setWaypointRepository(repository);
    }
}

void DashboardCentralWidget::setCameraFeedProvider(CameraFeedProvider *provider)
{
    if (m_remoteControlPage != nullptr) {
        m_remoteControlPage->setCameraFeedProvider(provider);
    }
    if (m_liveControlPage != nullptr) {
        m_liveControlPage->setCameraFeedProvider(provider);
    }
    if (provider != nullptr && m_operationsOverviewPage != nullptr) {
        connect(provider, &CameraFeedProvider::connectionStateChanged, m_operationsOverviewPage, &OperationsOverviewPage::applyCameraConnection);
    }
    if (provider != nullptr && m_headerBar != nullptr) {
        connect(provider, &CameraFeedProvider::connectionStateChanged, m_headerBar, &HeaderBar::setCameraConnected);
    }

}

void DashboardCentralWidget::setTelemetryProvider(TelemetryProvider *provider)
{
    if (m_liveControlPage != nullptr) {
        m_liveControlPage->setTelemetryProvider(provider);
    }
}

void DashboardCentralWidget::setSensorTelemetryProvider(SensorTelemetryProvider *provider)
{
    if (m_liveControlPage != nullptr) {
        m_liveControlPage->setSensorTelemetryProvider(provider);
    }
}

void DashboardCentralWidget::setDrivingRecordService(DrivingRecordService *service)
{
    if (m_liveControlPage != nullptr) {
        m_liveControlPage->setDrivingRecordService(service);
    }


bool DashboardCentralWidget::handleManualKeyPress(int key)
{
    if (m_slamMappingPage != nullptr && m_stack->currentWidget() == m_slamMappingPage) {
        return m_slamMappingPage->handleManualKeyPress(key);
    }
    return false;
}

bool DashboardCentralWidget::handleManualKeyRelease(int key)
{
    if (m_slamMappingPage != nullptr && m_stack->currentWidget() == m_slamMappingPage) {
        return m_slamMappingPage->handleManualKeyRelease(key);
    }
    return false;
}

void DashboardCentralWidget::addPage(PageId id, QWidget *page)
{
    const int index = m_stack->addWidget(page);
    m_pageIndex.insert(id, index);
}

void DashboardCentralWidget::navigateTo(PageId page)
{
    if (!m_pageIndex.contains(page)) {
        return;
    }

    m_stack->setCurrentIndex(m_pageIndex.value(page));
    m_headerBar->setPageTitle(titleForPage(page));
    m_sideNav->setActivePage(page);
}

QString DashboardCentralWidget::titleForPage(PageId page) const
{
    switch (page) {
    case PageId::OperationsOverview:
        return QStringLiteral("운영 현황");
    case PageId::LiveControl:
        return QStringLiteral("실시간 관제");
    case PageId::MissionOps:
        return QStringLiteral("미션 관제");
    case PageId::RemoteControl:
        return QStringLiteral("원격 제어");
    case PageId::SlamMapping:
        return QStringLiteral("지도 생성 (SLAM)");
    case PageId::WaypointManagement:
        return QStringLiteral("웨이포인트 관리");
    case PageId::RecordsPlayback:
        return QStringLiteral("주행 기록");
    case PageId::CallHistory:
        return QStringLiteral("호출 이력");
    case PageId::UserManagement:
        return QStringLiteral("사용자 관리");
    case PageId::RobotManagement:
        return QStringLiteral("로봇 관리");
    case PageId::SystemSettings:
        return QStringLiteral("시스템 설정");
    }
    return QString();
}
