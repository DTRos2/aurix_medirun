#include "dashboardcentralwidget.h"

#include "headerbar.h"
#include "sidenav.h"

#include "../pages/callhistorypage.h"
#include "../pages/livecontrolpage.h"
#include "../pages/missionopspage.h"
#include "../pages/operationsoverviewpage.h"
#include "../pages/recordsplaybackpage.h"
#include "../pages/remotecontrolpage.h"
#include "../pages/slammappingpage.h"
#include "../pages/systemsettingspage.h"
#include "../pages/usermanagementpage.h"
#include "../pages/waypointmanagementpage.h"

#include <QHBoxLayout>
#include <QStackedWidget>
#include <QVBoxLayout>

DashboardCentralWidget::DashboardCentralWidget(const AppSession &session, AuthService &authService, QWidget *parent)
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

    auto *liveControl = new LiveControlPage(m_stack);
    connect(liveControl, &LiveControlPage::missionCancelRequested, this, &DashboardCentralWidget::missionCancelRequested);
    connect(liveControl, &LiveControlPage::returnHomeRequested, this, &DashboardCentralWidget::returnHomeRequested);
    addPage(PageId::LiveControl, liveControl);

    auto *missionOps = new MissionOpsPage(m_stack);
    connect(missionOps, &MissionOpsPage::missionCancelRequested, this, &DashboardCentralWidget::missionCancelRequested);
    connect(missionOps, &MissionOpsPage::returnHomeRequested, this, &DashboardCentralWidget::returnHomeRequested);
    connect(missionOps, &MissionOpsPage::remoteControlRequested, this, [this]() { navigateTo(PageId::RemoteControl); });
    addPage(PageId::MissionOps, missionOps);

    addPage(PageId::RecordsPlayback, new RecordsPlaybackPage(m_stack));

    auto *callHistory = new CallHistoryPage(m_stack);
    connect(callHistory, &CallHistoryPage::replayRequested, this, [this]() { navigateTo(PageId::RecordsPlayback); });
    addPage(PageId::CallHistory, callHistory);

    addPage(PageId::OperationsOverview, new OperationsOverviewPage(m_stack));
    addPage(PageId::RemoteControl, new RemoteControlPage(m_stack));

    if (m_session.role == UserRole::Admin) {
        addPage(PageId::SlamMapping, new SlamMappingPage(m_stack));
        addPage(PageId::WaypointManagement, new WaypointManagementPage(m_stack));
        addPage(PageId::UserManagement, new UserManagementPage(m_authService, m_stack));
        addPage(PageId::SystemSettings, new SystemSettingsPage(m_stack));
    }

    navigateTo(PageId::OperationsOverview);
}

void DashboardCentralWidget::applySnapshot(const TelemetrySnapshot &snapshot)
{
    m_headerBar->applySnapshot(snapshot);
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
    case PageId::SystemSettings:
        return QStringLiteral("시스템 설정");
    }
    return QString();
}
