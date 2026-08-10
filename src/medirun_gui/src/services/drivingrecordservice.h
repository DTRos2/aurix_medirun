#ifndef DRIVINGRECORDSERVICE_H
#define DRIVINGRECORDSERVICE_H

#include "../app/appsession.h"
#include "../models/telemetrymodel.h"
#include "videorecorder.h"

#include <QImage>
#include <QObject>
#include <memory>

class QTimer;
class NavigationService;
class TelemetryProvider;
class CameraFeedProvider;
class RobotService;
class OperationsOverviewService;
class WaypointRepository;

// 실시간 관제의 "녹화 중지/시작" 버튼 뒤에서 driving_session/driving_record
// (db/schema/postgresql_schema.sql)에 주기적으로 로봇 위치/속도/배터리를 적재하고,
// 동시에 카메라 프레임을 mp4로 인코딩해 camera_session/camera_recording/files에
// 남긴다. NavGoalLogRecorder와 같은 원칙 — 등록된 로봇이 정확히 1대일 때만 기록한다.
class DrivingRecordService : public QObject
{
    Q_OBJECT

public:
    explicit DrivingRecordService(
        const AppSession &session,
        NavigationService *navigationService,
        TelemetryProvider *telemetryProvider,
        CameraFeedProvider *cameraFeedProvider,
        WaypointRepository *waypointRepository,
        RobotService &robotService,
        OperationsOverviewService &overviewService,
        QObject *parent = nullptr);

    bool isRecording() const;
    bool isPaused() const;

public slots:
    void startRecording();
    void stopRecording();
    void pauseRecording();
    void resumeRecording();

signals:
    void recordingStateChanged(bool recording);
    void recordingPausedChanged(bool paused);
    // 저장 결과를 UI가 사용자에게 즉시 보여줄 수 있도록 성공 여부와, 성공 시
    // 저장된 영상 경로/실패 시 실패 사유를 담아 내보낸다.
    void recordingSaveFinished(bool ok, const QString &message);

private slots:
    void handleAmclPose(double x, double y, double yawRad);
    void handleOdomVelocity(double linearMps, double angularRadps);
    void handleTelemetrySnapshot(const TelemetrySnapshot &snapshot);
    void handleCameraFrame(const QImage &frame);
    void appendSample();

private:
    AppSession m_session;
    WaypointRepository *m_waypointRepository {nullptr};
    RobotService &m_robotService;
    OperationsOverviewService &m_overviewService;
    QTimer *m_sampleTimer {nullptr};
    bool m_recording {false};
    bool m_paused {false};
    int m_drivingSessionId {-1};
    int m_robotSessionId {-1};
    int m_robotId {-1};
    int m_cameraSessionId {-1};
    std::unique_ptr<VideoRecorder> m_videoRecorder;
    QString m_videoFilePath;

    bool m_hasPose {false};
    double m_lastX {0.0};
    double m_lastY {0.0};
    double m_lastYaw {0.0};
    double m_lastLinearVel {0.0};
    double m_lastAngularVel {0.0};
    int m_lastBatteryPercent {-1};
};

#endif // DRIVINGRECORDSERVICE_H
