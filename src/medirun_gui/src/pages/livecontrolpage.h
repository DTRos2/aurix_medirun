#ifndef LIVECONTROLPAGE_H
#define LIVECONTROLPAGE_H

#include "../app/appsession.h"
#include "../models/telemetrymodel.h"
#include "../services/mapfile.h"

#include <QImage>
#include <QVector>
#include <QWidget>

class MapViewWidget;
class QLabel;
class QPushButton;
class QVBoxLayout;
class QFrame;
class QTimer;
class MissionController;
class NavigationService;
class TelemetryProvider;
class SensorTelemetryProvider;
class CameraFeedProvider;
class WaypointRepository;
class OperationsOverviewService;
class DrivingRecordService;

class LiveControlPage : public QWidget
{
    Q_OBJECT

public:
    explicit LiveControlPage(QWidget *parent = nullptr);

    void setSession(const AppSession &session);
    void setMissionController(MissionController *controller);
    void setNavigationService(NavigationService *service);
    void setTelemetryProvider(TelemetryProvider *provider);
    void setSensorTelemetryProvider(SensorTelemetryProvider *provider);
    void setCameraFeedProvider(CameraFeedProvider *provider);
    void setWaypointRepository(WaypointRepository *repository);
    void setOperationsOverviewService(OperationsOverviewService *service);
    void setDrivingRecordService(DrivingRecordService *service);

signals:
    void missionCancelRequested();
    void returnHomeRequested();
    void missionOpsRequested();

private slots:
    void refreshMissionStepper();
    void refreshWaitingCallBanner();
    void refreshEventStream();
    void refreshMap();
    void handleAmclPoseUpdated(double x, double y, double yawRad);
    void handleOdomVelocityUpdated(double linearMps, double angularRadps);
    void handleTelemetrySnapshot(const TelemetrySnapshot &snapshot);
    void handleFrontDistanceChanged(int centimeters);
    void handleRearDistanceChanged(int centimeters);
    void handlePayloadLoadedChanged(bool loaded);
    void handleNav2ConnectionChanged(bool connected);
    void handleCameraFrameReady(const QImage &frame);
    void handleRecordingStateChanged(bool recording);
    void handleRecordingPausedChanged(bool paused);
    void handleRecordingSaveFinished(bool ok, const QString &message);
    void handleRecordButtonClicked();
    void handleSaveRecordingButtonClicked();
    void handlePauseRecordingButtonClicked();
    void handleSnapshotButtonClicked();
    void tickRecordingElapsed();

private:
    QWidget *buildMissionStepper();
    QWidget *buildActionRow();
    QWidget *buildRobotStatusPanel();
    QWidget *buildEventStreamPanel();
    QWidget *buildWaitingCallBanner();
    QWidget *buildCameraPanel();
    void loadDefaultMap();
    void refreshMapMarkers();
    QPointF worldToNormalized(double x, double y) const;
    void updateNav2StatusLabel();
    void updateSpeedLabel();
    void replaceBatteryBar(int percent);

    MapViewWidget *m_mapView;
    QLabel *m_cameraViewLabel {nullptr};
    // 주행기록 페이지의 재생바와 같은 모양(막대+표시줄)을 녹화 중에만 카메라
    // 패널 하단에 보여준다. 실시간 녹화라 되감기/배속 같은 탐색 조작은 의미가
    // 없어서, 진행률 막대는 "녹화 중" 표시로, 재생 시간 대신 경과 시간만 둔다.
    QWidget *m_cameraTimelinePanel {nullptr};
    QLabel *m_recordingElapsedLabel {nullptr};
    QPushButton *m_pauseRecordingButton {nullptr};
    QTimer *m_recordingElapsedTimer {nullptr};
    int m_recordingElapsedSeconds {0};

    AppSession m_session;
    MissionController *m_missionController {nullptr};
    NavigationService *m_navigationService {nullptr};
    TelemetryProvider *m_telemetryProvider {nullptr};
    SensorTelemetryProvider *m_sensorTelemetryProvider {nullptr};
    CameraFeedProvider *m_cameraFeedProvider {nullptr};
    WaypointRepository *m_waypointRepository {nullptr};
    OperationsOverviewService *m_operationsOverviewService {nullptr};
    DrivingRecordService *m_drivingRecordService {nullptr};

    MapMeta m_mapMeta;

    // 미션 스테퍼
    QLabel *m_missionInfoLabel {nullptr};
    QLabel *m_missionMetaLabel {nullptr};
    QVector<QLabel *> m_stepCircles;
    QVector<QLabel *> m_stepLabels;
    QVector<QFrame *> m_stepConnectors;

    // 액션 로우
    QPushButton *m_recordButton {nullptr};
    QPushButton *m_saveRecordingButton {nullptr};
    QPushButton *m_snapshotButton {nullptr};
    QImage m_lastCameraFrame;

    // 로봇 상태 패널
    QFrame *m_batteryBarHost {nullptr};
    QWidget *m_batteryBar {nullptr};
    QLabel *m_batteryValueLabel {nullptr};
    QLabel *m_payloadValueLabel {nullptr};
    QLabel *m_obstacleValueLabel {nullptr};
    QLabel *m_rearObstacleValueLabel {nullptr};
    QLabel *m_nav2ValueLabel {nullptr};
    QLabel *m_speedValueLabel {nullptr};
    int m_lastBatteryPercent {-1};
    // 배터리 바는 값이 실제로 바뀔 때만 다시 만든다 — 매 스냅샷마다(값이 같아도)
    // 새로 만들면 위젯이 계속 교체되면서 깜빡여 보인다.
    int m_lastDisplayedBatteryPercent {-2};
    bool m_lastNav2Connected {false};
    // /odom은 수십 Hz로 오고 값 자체도 흔들려서, 매 콜백마다 라벨을 갱신하면
    // 숫자가 떨려 보인다. 지수平균으로 값을 다듬고, 화면 갱신은 타이머로 늦춘다.
    double m_lastLinearVel {0.0};
    QTimer *m_speedUpdateTimer {nullptr};

    // 이벤트 스트림
    QVBoxLayout *m_eventStreamLayout {nullptr};
    QTimer *m_eventPollTimer {nullptr};

    // 대기 호출 배너
    QLabel *m_waitingCallLabel {nullptr};
};

#endif // LIVECONTROLPAGE_H
