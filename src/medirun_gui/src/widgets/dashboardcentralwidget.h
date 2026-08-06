#ifndef DASHBOARDCENTRALWIDGET_H
#define DASHBOARDCENTRALWIDGET_H

#include "../app/appsession.h"
#include "../models/telemetrymodel.h"
#include "pageid.h"

#include <QHash>
#include <QWidget>

class AuthService;
class HeaderBar;
class SideNav;
class QStackedWidget;
class NavigationService;
class WaypointManagementPage;
class RemoteControlPage;
class CameraFeedProvider;
class SlamMappingPage;
class MissionController;
class WaypointRepository;
class MissionOpsPage;

class DashboardCentralWidget : public QWidget
{
    Q_OBJECT

public:
    explicit DashboardCentralWidget(const AppSession &session, AuthService &authService, QWidget *parent = nullptr);

    void applySnapshot(const TelemetrySnapshot &snapshot);
    void setNavigationService(NavigationService *service);
    void setCameraFeedProvider(CameraFeedProvider *provider);
    void setMissionController(MissionController *controller);
    void setWaypointRepository(WaypointRepository *repository);
    bool handleManualKeyPress(int key);
    bool handleManualKeyRelease(int key);

signals:
    void exitRequested();
    void estopRequested();
    void missionCancelRequested();
    void returnHomeRequested();
    void logoutRequested();

private:
    void addPage(PageId id, QWidget *page);
    void navigateTo(PageId page);
    QString titleForPage(PageId page) const;

    AuthService &m_authService;
    AppSession m_session;
    HeaderBar *m_headerBar;
    SideNav *m_sideNav;
    QStackedWidget *m_stack;
    QHash<PageId, int> m_pageIndex;
    WaypointManagementPage *m_waypointPage {nullptr};
    RemoteControlPage *m_remoteControlPage {nullptr};
    SlamMappingPage *m_slamMappingPage {nullptr};
    MissionOpsPage *m_missionOpsPage {nullptr};
};

#endif // DASHBOARDCENTRALWIDGET_H
