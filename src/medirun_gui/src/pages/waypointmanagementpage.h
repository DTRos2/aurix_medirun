#ifndef WAYPOINTMANAGEMENTPAGE_H
#define WAYPOINTMANAGEMENTPAGE_H

#include "../models/waypointtypes.h"
#include "../ros/navigationservice.h"
#include "../services/mapfile.h"
#include "../services/operationsoverviewservice.h"
#include "../services/robotservice.h"
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
    explicit WaypointManagementPage(RobotService &robotService, OperationsOverviewService &overviewService, QWidget *parent = nullptr);

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
    void refreshNav2Badge();

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
    // 파일 없이도(다른 PC 등) 이 로봇으로 DB(waypoint_set)에 저장된 이름
    // 목록을 보여주고 고른 걸 불러온다.
    void handleLoadWaypointFromDb();
    void handleSaveWaypointFile();
    // call_button DB의 콜존(BTN_* 등)을 waypoints 위에 upsert한다. 대시보드
    // 최초 진입 화면(빈 상태) 및 "웨이포인트 불러오기" 직후 둘 다 이걸 거쳐야
    // call_button 등록분이 항상 지도에 나온다는 불변식이 성립한다.
    void mergeCallButtonsInto(WaypointMap &waypoints) const;
    // waypoint_set에 name="home"으로 저장된 라이브러리에서 kind=Home인
    // 항목만 골라 waypoints 위에 얹는다. mergeCallButtonsInto와 동일한
    // 지점(최초 진입 + 불러오기 직후)에서 같이 호출해 HOME도 항상 나오게 한다.
    void mergeHomeWaypointInto(WaypointMap &waypoints) const;
    // handleLoadWaypointFile/handleLoadWaypointFromDb가 공유 — 불러온
    // frame_id+waypoints+trajectories를(call_button과 합쳐서) 페이지 상태에
    // 반영하고 화면을 갱신한다.
    void applyLoadedWaypointLibrary(
        const QString &frameId, const WaypointMap &waypoints, const TrajectoryMap &trajectories, const QString &statusMessage);

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
    void handleNavSessionStarted();
    void handleNavSessionFinished();
    void syncFromRepository();
    void pushToRepository();

    // 참조 멤버라 생성자 초기화 목록에서 가장 먼저 바인딩돼야 하므로(-Wreorder
    // 방지) 선언 순서상 다른 멤버보다 위에 둔다.
    RobotService &m_robotService;
    OperationsOverviewService &m_overviewService;

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
    // amclPoseUpdated를 한 번이라도 받았는지 — AMCL은 초기 위치를 받기 전엔
    // /amcl_pose를 아예 발행하지 않으므로 "초기 위치 설정됨"의 대리 신호로
    // 쓴다. nav2Connected만으로는 실제 주행 가능 여부를 보장하지 못한다
    // (bt_navigator/waypoint_follower가 ACTIVE여도 로컬라이제이션이 없으면
    // 주행이 실패하거나 엉뚱하게 움직일 수 있다).
    bool m_hasAmclPose {false};
    // navSessionStarted~navSessionFinished 사이 true — 다른 웨이포인트/경로로
    // 이미 주행 중인데 또 새 목표를 보내면 이전 goal이 취소되지 않은 채
    // 두 번째 목표가 그대로 액션 서버로 전송돼(nav2navigationservice가
    // 자체적으로 선점 처리를 하지 않음) 로봇이 이전 목표를 향해 계속
    // 움직이고 DB 세션 기록도 꼬이는 문제가 있어, 주행 중엔 새 주행 시작
    // 버튼을 잠근다.
    bool m_navigating {false};

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
