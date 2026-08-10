#include "drivingrecordservice.h"

#include "camerafeedprovider.h"
#include "operationsoverviewservice.h"
#include "robotservice.h"
#include "telemetryprovider.h"
#include "waypointrepository.h"
#include "../ros/navigationservice.h"

#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QTimer>
#include <QUuid>

namespace {
constexpr int kSampleIntervalMs = 1000;
}

DrivingRecordService::DrivingRecordService(
    const AppSession &session,
    NavigationService *navigationService,
    TelemetryProvider *telemetryProvider,
    CameraFeedProvider *cameraFeedProvider,
    WaypointRepository *waypointRepository,
    RobotService &robotService,
    OperationsOverviewService &overviewService,
    QObject *parent)
    : QObject(parent)
    , m_session(session)
    , m_waypointRepository(waypointRepository)
    , m_robotService(robotService)
    , m_overviewService(overviewService)
    , m_sampleTimer(new QTimer(this))
{
    m_sampleTimer->setInterval(kSampleIntervalMs);
    connect(m_sampleTimer, &QTimer::timeout, this, &DrivingRecordService::appendSample);

    if (navigationService != nullptr) {
        connect(navigationService, &NavigationService::amclPoseUpdated, this, &DrivingRecordService::handleAmclPose);
        connect(navigationService, &NavigationService::odomVelocityUpdated, this, &DrivingRecordService::handleOdomVelocity);
    }
    if (telemetryProvider != nullptr) {
        connect(telemetryProvider, &TelemetryProvider::snapshotUpdated, this, &DrivingRecordService::handleTelemetrySnapshot);
    }
    if (cameraFeedProvider != nullptr) {
        connect(cameraFeedProvider, &CameraFeedProvider::frameReady, this, &DrivingRecordService::handleCameraFrame);
    }
}

bool DrivingRecordService::isRecording() const
{
    return m_recording;
}

bool DrivingRecordService::isPaused() const
{
    return m_paused;
}

void DrivingRecordService::startRecording()
{
    if (m_recording) {
        return;
    }

    const QVector<RobotRecord> robots = m_robotService.listRobots(false);
    if (robots.size() != 1) {
        qWarning() << "DrivingRecordService::startRecording: 등록된 로봇이 정확히 1대가 아니라 건너뜀 (count="
                   << robots.size() << ")";
        return;
    }
    const int robotId = robots.first().id;

    int drivingSessionId = -1;
    int robotSessionId = -1;
    QString errorMessage;
    if (!m_overviewService.startDrivingSession(robotId, drivingSessionId, robotSessionId, errorMessage)) {
        qWarning() << "DrivingRecordService::startRecording 실패:" << errorMessage;
        return;
    }

    m_drivingSessionId = drivingSessionId;
    m_robotSessionId = robotSessionId;
    m_robotId = robotId;
    m_recording = true;
    m_paused = false;
    m_sampleTimer->start();

    // 카메라 녹화는 실패해도(카메라 미연결 등) 위치/속도 녹화 자체는 계속 진행한다 —
    // 하나가 안 된다고 나머지 녹화까지 막을 이유는 없음.
    m_cameraSessionId = -1;
    m_videoFilePath.clear();
    if (m_waypointRepository != nullptr) {
        const QString recordingsDir = m_waypointRepository->workspaceRootPath() + QStringLiteral("/recordings");
        QDir().mkpath(recordingsDir);
        const QString recordingUuid = QUuid::createUuid().toString(QUuid::WithoutBraces);
        const QString recordingDate = QDate::currentDate().toString(QStringLiteral("yyyyMMdd"));
        m_videoFilePath = recordingsDir + QStringLiteral("/rec_%1_%2.mp4").arg(recordingDate, recordingUuid);

        // camera_session은 (session_id, camera_name) UNIQUE 제약이 있는데, robot_session이
        // 'running' 상태로 유지되는 동안(예: 미션 진행 중) 같은 session_id로 녹화를 여러 번
        // 시작/저장하면 고정 이름("front_camera")이 매번 충돌해 두 번째 녹화부터
        // 카메라 세션 생성이 조용히 실패했었다 — 녹화마다 고유한 이름을 써서 회피한다.
        const QString cameraName = QStringLiteral("front_camera_%1").arg(recordingUuid.left(8));

        QString cameraErrorMessage;
        if (m_overviewService.startCameraSession(
                robotSessionId, drivingSessionId, cameraName, QStringLiteral("camera/image_raw"),
                m_cameraSessionId, cameraErrorMessage)) {
            m_videoRecorder = std::make_unique<VideoRecorder>();
            m_videoRecorder->start(m_videoFilePath);
        } else {
            qWarning() << "DrivingRecordService: 카메라 세션 생성 실패:" << cameraErrorMessage;
        }
    }

    emit recordingStateChanged(true);

    QString userActionError;
    if (!m_overviewService.recordUserAction(
            robotId, robotSessionId > 0 ? robotSessionId : -1, m_session.userId, QStringLiteral("recording_start"),
            QString(), m_session.username, userActionError)) {
        qWarning() << "DrivingRecordService: recording_start 사용자 조작 로그 실패:" << userActionError;
    }
}

void DrivingRecordService::stopRecording()
{
    if (!m_recording) {
        return;
    }

    m_sampleTimer->stop();
    m_recording = false;
    const bool wasPaused = m_paused;
    m_paused = false;
    if (wasPaused) {
        emit recordingPausedChanged(false);
    }
    emit recordingStateChanged(false);

    bool videoSaved = true;
    QString videoErrorMessage;
    const bool hadVideoRecorder = (m_videoRecorder != nullptr);

    if (m_videoRecorder != nullptr) {
        const VideoRecorder::Result result = m_videoRecorder->stop();
        m_videoRecorder.reset();

        if (result.wroteAnyFrame && m_cameraSessionId > 0) {
            int cameraRecordingId = -1;
            QString errorMessage;
            if (m_overviewService.recordCameraRecording(
                    m_robotSessionId, m_cameraSessionId, result.durationSec, result.width, result.height, result.fps,
                    QStringLiteral("mp4v"), cameraRecordingId, errorMessage)) {
                QString fileErrorMessage;
                if (!m_overviewService.recordFile(
                        QStringLiteral("camera_recording"), QString::number(cameraRecordingId), QStringLiteral("video"),
                        m_videoFilePath, QFileInfo(m_videoFilePath).fileName(), QStringLiteral("video/mp4"), m_session.username,
                        fileErrorMessage)) {
                    videoSaved = false;
                    videoErrorMessage = fileErrorMessage;
                }
            } else {
                qWarning() << "DrivingRecordService: 카메라 녹화 기록 실패:" << errorMessage;
                videoSaved = false;
                videoErrorMessage = errorMessage;
            }
        } else if (!result.wroteAnyFrame) {
            videoSaved = false;
            videoErrorMessage = QStringLiteral("녹화된 프레임이 없습니다 (카메라 연결을 확인하세요).");
        }
    }
    m_cameraSessionId = -1;

    bool sessionSaved = true;
    QString sessionErrorMessage;
    if (m_drivingSessionId > 0) {
        QString errorMessage;
        if (!m_overviewService.finishDrivingSession(m_drivingSessionId, errorMessage)) {
            qWarning() << "DrivingRecordService::stopRecording 실패:" << errorMessage;
            sessionSaved = false;
            sessionErrorMessage = errorMessage;
        }
    }
    m_drivingSessionId = -1;

    const bool ok = videoSaved && sessionSaved;
    const QString resultMessage = ok
        ? (hadVideoRecorder ? m_videoFilePath : QStringLiteral("주행 기록이 저장되었습니다."))
        : (!videoErrorMessage.isEmpty() ? videoErrorMessage : sessionErrorMessage);
    emit recordingSaveFinished(ok, resultMessage);

    if (m_robotId > 0) {
        QString userActionError;
        if (!m_overviewService.recordUserAction(
                m_robotId, m_robotSessionId > 0 ? m_robotSessionId : -1, m_session.userId, QStringLiteral("recording_stop"),
                QString(), m_session.username, userActionError)) {
            qWarning() << "DrivingRecordService: recording_stop 사용자 조작 로그 실패:" << userActionError;
        }
    }
    m_robotSessionId = -1;
}

void DrivingRecordService::pauseRecording()
{
    if (!m_recording || m_paused) {
        return;
    }
    m_paused = true;
    m_sampleTimer->stop();
    emit recordingPausedChanged(true);
}

void DrivingRecordService::resumeRecording()
{
    if (!m_recording || !m_paused) {
        return;
    }
    m_paused = false;
    m_sampleTimer->start();
    emit recordingPausedChanged(false);
}

void DrivingRecordService::handleAmclPose(double x, double y, double yawRad)
{
    m_hasPose = true;
    m_lastX = x;
    m_lastY = y;
    m_lastYaw = yawRad;
}

void DrivingRecordService::handleOdomVelocity(double linearMps, double angularRadps)
{
    m_lastLinearVel = linearMps;
    m_lastAngularVel = angularRadps;
}

void DrivingRecordService::handleTelemetrySnapshot(const TelemetrySnapshot &snapshot)
{
    m_lastBatteryPercent = snapshot.batteryPercent;
}

void DrivingRecordService::handleCameraFrame(const QImage &frame)
{
    if (!m_recording || m_paused || m_videoRecorder == nullptr) {
        return;
    }
    m_videoRecorder->writeFrame(frame);
}

void DrivingRecordService::appendSample()
{
    if (!m_recording || m_drivingSessionId <= 0 || !m_hasPose) {
        return;
    }

    QString errorMessage;
    if (!m_overviewService.appendDrivingRecord(
            m_drivingSessionId, m_lastX, m_lastY, m_lastYaw, m_lastLinearVel, m_lastAngularVel,
            m_lastBatteryPercent, errorMessage)) {
        qWarning() << "DrivingRecordService::appendSample 실패:" << errorMessage;
    }
}
