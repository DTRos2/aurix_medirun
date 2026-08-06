#ifndef WAYPOINTMANAGEMENTPAGE_H
#define WAYPOINTMANAGEMENTPAGE_H

#include "../models/waypointtypes.h"
#include "../ros/navigationservice.h"
#include "../services/mapfile.h"
#include "../widgets/waypointmapwidget.h"

#include <QVector>
#include <QWidget>

class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QPushButton;
class QStackedWidget;
class WaypointRepository;

class WaypointManagementPage : public QWidget
{
    Q_OBJECT

public:
    explicit WaypointManagementPage(QWidget *parent = nullptr);

    // Injected by GuiApplicationController (nullable — nav buttons stay
    // disabled and just report an error if no service is attached).
    void setNavigationService(NavigationService *service);
    void setWaypointRepository(WaypointRepository *repository);

private:
    QWidget *buildHeader();
    QWidget *buildWarningBanner();
    QWidget *buildWaypointTab();
    QWidget *buildTrajectoryTab();

    QWidget *buildWaypointListPanel();
    QWidget *buildWaypointEditPanel();
    QWidget *buildRouteAddPanel();
    QWidget *buildRouteOrderPanel();
    QWidget *buildRouteNamePanel();
    QWidget *buildSavedRoutesBar();

    void selectTab(int index);
    void refreshWaypointList();
    void refreshMapMarkers();
    void refreshRouteCombo();
    void refreshSavedRoutesBar();
    void renumberRouteOrderItems();
    void updateNavButtonsEnabled();

    void selectWaypoint(const QString &id);
    void loadWaypointIntoForm(const WaypointDef &record);
    void handleNewWaypointRequested();
    void prefillNewWaypointForm(double x, double y, double yawDeg);
    void handleSaveWaypoint();
    void handleDeleteWaypoint();
    void handleMapPickRequested(const QPointF &normalizedPos, double yawRadians);
    QString nextIdForKind(WaypointMarkerKind kind) const;
    QPointF normalizedFromMeters(double x, double y) const;
    QPointF worldToNormalized(double x, double y) const;
    QPointF normalizedToWorld(const QPointF &normalized) const;

    void handleAddToRoute();
    void handleRemoveSelectedFromRoute();
    void handleClearRoute();
    void handleSaveTrajectory();
    void handleSavedRouteChipClicked(const QString &name);
    void handleDeleteSelectedSavedRoute();
    void handleLoadMap();
    void handleClearMap();
    // Shared by handleLoadMap and the startup auto-load: parses the YAML at
    // path and, on success, applies it to both map widgets + status label.
    bool loadMapFromFile(const QString &path, QString &errorMessage);
    // Best-effort: on startup, load the same default map medirun_bringup
    // passes to Nav2 (<workspace>/maps/last_class_map_modi.yaml) so the
    // robot pose marker (AMCL) has something to render onto without the
    // user having to click "지도 불러오기" manually. Silently does nothing
    // if the file isn't found (e.g. running outside the install layout).
    void autoLoadDefaultMap();
    // <workspace>/install/.../lib/medirun_gui에서 4단계 위로 올라간 경로.
    // 계산 불가 시(설치 레이아웃 밖에서 실행 등) 빈 문자열.
    QString workspaceRootPath() const;
    void handleLoadWaypointFile();
    void handleSaveWaypointFile();

    void handleNavigateToSelectedWaypoint();
    void handleNavigateSelectedRoute();
    void handleCancelNavigation();
    void handleSetInitialPoseToggled(bool checked);
    void showSpeedPopup();
    void applySpeedLimit(int percent);

    void handleNavStatusChanged(const QString &text);
    void handleNav2ConnectionChanged(bool connected);
    void handleNavFeedback(double metersRemaining);
    void handleNavWaypointProgress(int currentIndex, int total);
    void handleAmclPoseUpdated(double x, double y, double yawRad);
    void handleScanPointsUpdated(const QVector<QPointF> &mapPoints);
    void syncFromRepository();
    void pushToRepository();

    WaypointMap m_waypoints;
    TrajectoryMap m_trajectories;
    QString m_frameId {QStringLiteral("map")};
    QString m_waypointFilePath;
    MapMeta m_currentMapMeta;

    QString m_selectedWaypointId;
    bool m_creatingNew;
    // true while waiting for the next map click to be interpreted as a
    // "set initial pose" pick (equivalent to RViz's 2D Pose Estimate)
    // rather than a new-waypoint pick.
    bool m_settingInitialPose;
    // Last speed limit percent applied (1-100), shown on m_speedButton and
    // preselected the next time the speed popup is opened.
    int m_speedPercent {100};

    QString m_selectedSavedRoute;

    NavigationService *m_navigationService {nullptr};
    WaypointRepository *m_waypointRepository {nullptr};
    // navigate_to_pose/follow_waypoints 액션 서버가 실제로 goal을 받을 수
    // 있는지(nav2ConnectionChanged로 갱신). SLAM 매핑 중 nav2가 PAUSE되면
    // false가 되어 주행 버튼이 잠긴다.
    bool m_nav2Connected {false};

    // shell
    QPushButton *m_waypointTabButton;
    QPushButton *m_trajectoryTabButton;
    QStackedWidget *m_tabStack;
    QLabel *m_mapStatusLabel;
    QLabel *m_navStatusLabel;
    QLabel *m_nav2ConnectionBadge {nullptr};
    QPushButton *m_setInitialPoseButton;
    QPushButton *m_speedButton;

    // waypoint tab
    WaypointMapWidget *m_waypointMap;
    QListWidget *m_waypointList;
    QLabel *m_waypointListCountLabel;
    QLabel *m_editPanelTitle;
    QLineEdit *m_nameEdit;
    QLineEdit *m_xEdit;
    QLineEdit *m_yEdit;
    QLineEdit *m_thetaEdit;
    QComboBox *m_kindCombo;
    QPushButton *m_saveWaypointButton;
    QPushButton *m_deleteWaypointButton;
    QPushButton *m_navigateButton;
    QPushButton *m_cancelNavButton;
    QLabel *m_waypointStatusLabel;

    // trajectory tab
    WaypointMapWidget *m_routeMap;
    QComboBox *m_routeAddCombo;
    QListWidget *m_routeOrderList;
    QLabel *m_routeOrderCountLabel;
    QLineEdit *m_routeNameEdit;
    QWidget *m_savedRoutesBar;
    QPushButton *m_navigateRouteButton;
    QLabel *m_trajectoryStatusLabel;
};

#endif // WAYPOINTMANAGEMENTPAGE_H
