#ifndef REMOTECONTROLPAGE_H
#define REMOTECONTROLPAGE_H

#include "../services/mapfile.h"
#include "../services/teleopcontroller.h"

#include <QHideEvent>
#include <QImage>
#include <QShowEvent>
#include <QWidget>

class QVBoxLayout;
class QLabel;
class QPushButton;
class MapViewWidget;
class CameraFeedProvider;
class NavigationService;
class RobotService;
class OperationsOverviewService;

class RemoteControlPage : public QWidget
{
    Q_OBJECT

public:
    explicit RemoteControlPage(RobotService &robotService, OperationsOverviewService &overviewService, QWidget *parent = nullptr);

    void setCameraFeedProvider(CameraFeedProvider *provider);
    void setNavigationService(NavigationService *service);

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

private:
    QWidget *buildManualControlBanner();
    QWidget *buildCameraPanel();
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
    QWidget *m_cameraBottomRow = nullptr;
    MapViewWidget *m_mapView = nullptr;
    // 지도가 축소된(미니맵) 상태일 때는 감춰서, 확대(메인) 상태일 때만 보인다.
    QWidget *m_mapPoseHintBanner = nullptr;
    QPushButton *m_setInitialPoseButton = nullptr;
    QVBoxLayout *m_bigSlot = nullptr;
    QVBoxLayout *m_smallSlot = nullptr;
    bool m_mapIsMain = false;

    CameraFeedProvider *m_cameraFeedProvider = nullptr;
    QImage m_lastCameraFrame;

    NavigationService *m_navigationService = nullptr;
    MapMeta m_currentMapMeta;
    bool m_settingInitialPose = false;

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
};

#endif // REMOTECONTROLPAGE_H
