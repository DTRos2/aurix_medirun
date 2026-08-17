#ifndef REMOTECONTROLPAGE_H
#define REMOTECONTROLPAGE_H

#include "../app/appsession.h"
#include "../app/appsession.h"
#include "../ros/navigationservice.h"
#include "../services/mapfile.h"
#include "../services/teleopcontroller.h"

#include <QHideEvent>
#include <QImage>
#include <QShowEvent>
#include <QVector>
#include <QWidget>

class QVBoxLayout;
class QLabel;
class QPushButton;
class MapViewWidget;
class CameraFeedProvider;
class RobotService;
class OperationsOverviewService;
class SensorTelemetryProvider;

class RemoteControlPage : public QWidget
{
    Q_OBJECT

public:
    explicit RemoteControlPage(RobotService &robotService, OperationsOverviewService &overviewService, QWidget *parent = nullptr);

    void setCameraFeedProvider(CameraFeedProvider *provider);
    void setNavigationService(NavigationService *service);
    void setSensorTelemetryProvider(SensorTelemetryProvider *provider);
    void setSession(const AppSession &session);

    // MainWindow -> DashboardCentralWidget이 이 페이지가 현재 화면일 때만
    // 위임한다 (SlamMappingPage와 동일한 외부 인터페이스).
    bool handleManualKeyPress(int key);
    bool handleManualKeyRelease(int key);

protected:
    void resizeEvent(QResizeEvent *event) override;
    void hideEvent(QHideEvent *event) override;
    void showEvent(QShowEvent *event) override;

private slots:
    void applyCameraFrame(const QImage &frame);
    void applyCameraConnectionState(bool connected);
    void handleAmclPoseUpdated(double x, double y, double yawRad);
    void handleTeleopVelocityChanged(double linearX, double angularZ);
    void handleTeleopActiveChanged(bool active);
    void handleTeleopSpeedPresetChanged(TeleopController::SpeedPreset preset);
    // teleop이 직접 조작 중이 아닐 때(웨이포인트 자동주행 중이거나 대기
    // 중일 때)의 cmd_vel 패널 갱신 — 실제 /odom 속도를 보여준다. teleop
    // 조작 중에는 즉시성이 더 중요해 handleTeleopVelocityChanged가 그대로
    // 우선한다(이미 실기 검증된 경로라 건드리지 않는다).
    void handleOdomVelocityUpdated(double linearMps, double angularRadps);
    // 웨이포인트 관리 페이지 등 다른 곳에서 시작한 주행이라도, 같은
    // NavigationService를 공유하므로 이 시그널로 감지해서 수동 제어를 막는다.
    void handleNavSessionStarted(const QVector<NavPose> &goals, const QString &label);
    void handleNavSessionFinished(bool success, bool cancelled);
    void handleFrontDistanceChanged(int centimeters);
    void handleRearDistanceChanged(int centimeters);
    void handlePayloadLoadedChanged(bool loaded);

private:
    QWidget *buildManualControlBanner();
    QWidget *buildCameraPanel();
    QWidget *buildSensorStatusPanel();
    QWidget *buildMinimapPanel();
    QWidget *buildPoseHintBanner();
    QWidget *buildViewSwapButton();
    QWidget *buildLoadMapButton();
    QWidget *buildRefreshMarkersButton();
    QWidget *buildDirectionPad();
    QWidget *buildSpeedPresetPanel();
    QWidget *buildSpeedAxisRow(const QString &label, const QString &unit, int percent, QLabel **valueLabelOut);
    QWidget *buildCmdVelPanel();
    void updateSpeedPresetButtons();

    void swapCameraAndMap();
    void handleLoadMap();
    bool loadMapFromFile(const QString &path, QString &errorMessage);
    QString workspaceRootPath() const;
    void autoLoadDefaultMap();
    void handleSetInitialPoseToggled(bool checked);
    void handlePosePickRequested(const QPointF &normalizedPos, double yawRadians);
    QPointF worldToNormalized(double x, double y) const;
    QPointF normalizedToWorld(const QPointF &normalized) const;
    void updateInitialPoseButtonEnabled();
    void updateInitialPoseBanner();
    // teleop 수동 조작 중이거나 웨이포인트 자동주행 중이면 로봇이 실제로
    // 움직이고 있는 것이므로 마커를 "주행 중"(Moving)으로, 둘 다 아니면
    // "대기"(Ready)로 되돌린다. AMCL 위치가 들어올 때뿐 아니라 teleop/자동주행
    // 시작·종료 시점에도 즉시 반영해야 값이 도착하기 전까지 마커가 낡은 상태로
    // 남지 않는다.
    void refreshRobotMarkerState();
    // 지도가 로드된 뒤 DB(waypoint_set.name="home", call_button)에서 홈/콜존
    // 마커를 읽어와 지도에 반영한다 — WaypointManagementPage의
    // mergeHomeWaypointInto/mergeCallButtonsInto와 동일한 source of truth.
    void refreshDbMarkers();

    RobotService &m_robotService;
    OperationsOverviewService &m_overviewService;

    QWidget *m_cameraPanel = nullptr;
    QWidget *m_minimapPanel = nullptr;
    QLabel *m_cameraFrameLabel = nullptr;
    // 카메라 패널이 축소된(스왑된) 상태일 때는 감춰서 영상 표시 영역을 넓게 확보한다.
    QWidget *m_cameraTopRow = nullptr;
    MapViewWidget *m_mapView = nullptr;
    // 지도가 축소된(미니맵) 상태일 때는 감춰서, 확대(메인) 상태일 때만 보인다.
    QWidget *m_mapPoseHintBanner = nullptr;
    QLabel *m_poseHintIconLabel = nullptr;
    QLabel *m_poseHintTextLabel = nullptr;
    // AMCL이 초기 위치를 한 번이라도 받았는지 — 이게 false인 동안은 로봇 위치가
    // 신뢰할 수 없으므로 지도에 로봇 마커를 아예 그리지 않고, 대신 초기 위치
    // 설정 안내 배너(빨강)를 보여준다. 받은 뒤에는 위치 일치 확인 배너(주황)로
    // 바뀐다 — WaypointManagementPage::m_hasAmclPose와 동일한 규칙.
    bool m_hasAmclPose = false;
    QPushButton *m_setInitialPoseButton = nullptr;
    QVBoxLayout *m_bigSlot = nullptr;
    QVBoxLayout *m_smallSlot = nullptr;
    bool m_mapIsMain = false;

    CameraFeedProvider *m_cameraFeedProvider = nullptr;
    QImage m_lastCameraFrame;

    NavigationService *m_navigationService = nullptr;
    MapMeta m_currentMapMeta;
    bool m_settingInitialPose = false;
    // navSessionStarted~Finished 사이에는 웨이포인트로 자동 주행 중이므로
    // 수동 제어(방향 패드/WASD)를 막는다 — 자동주행과 teleop이 동시에
    // /cmd_vel을 발행하면 서로 충돌한다.
    bool m_navigationInProgress = false;
    QWidget *m_directionPad = nullptr;
    bool m_teleopActive = false;
    // /odom 콜백이 수십 Hz로 오고 특히 angular.z는 값 자체가 흔들려서,
    // LiveControlPage::handleOdomVelocityUpdated와 동일한 지수이동평균으로
    // 다듬은 뒤 표시한다.
    double m_lastOdomLinearVel = 0.0;
    double m_lastOdomAngularVel = 0.0;
    // 배너에 실제 주행 중인 계정을 보여주기 위한 것 — 여러 PC 동시 조작을
    // 막는 제어권 락은 아직 없고(추후 별도 기능), 지금은 "지금 이 PC에서
    // 로그인한 계정"만 표시용으로 쓴다.
    AppSession m_session;

    TeleopController *m_teleop = nullptr;
    QLabel *m_manualBannerTitleLabel = nullptr;
    QLabel *m_manualBannerCaptionLabel = nullptr;
    QPushButton *m_speedFineButton = nullptr;
    QPushButton *m_speedNormalButton = nullptr;
    QPushButton *m_speedFastButton = nullptr;
    QLabel *m_linearSpeedValueLabel = nullptr;
    QLabel *m_angularSpeedValueLabel = nullptr;
    QLabel *m_cmdVelTitleLabel = nullptr;
    QLabel *m_cmdVelLinearValueLabel = nullptr;
    QLabel *m_cmdVelAngularValueLabel = nullptr;

    // 센서 상태 패널 — LiveControlPage 로봇 상태박스의 전방/후방 장애물·AED
    // 적재(홀센서) 항목을 그대로 재사용한다. 좌/우 초음파 센서는 실제
    // 하드웨어가 없어 표시하지 않는다.
    SensorTelemetryProvider *m_sensorTelemetryProvider = nullptr;
    QLabel *m_frontObstacleValueLabel = nullptr;
    QLabel *m_rearObstacleValueLabel = nullptr;
    QLabel *m_payloadValueLabel = nullptr;
};

#endif // REMOTECONTROLPAGE_H
