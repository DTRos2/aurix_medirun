#ifndef MISSIONROSINTERFACE_H
#define MISSIONROSINTERFACE_H

#include <QObject>
#include <QDateTime>

#include <memory>

class MissionRosInterface : public QObject
{
    Q_OBJECT

public:
    explicit MissionRosInterface(QObject *parent = nullptr);
    ~MissionRosInterface() override;

public slots:
    void publishMissionCommand(const QString &command,
                               const QString &missionId,
                               const QString &queueId,
                               const QString &waypointId);
    // Pi에서 도는 robot_audio_output(audio_output_node)의 /audio/command로
    // TTS 문장을 발행한다 — bringup의 "주행을 시작합니다" 안내와 동일한 방식
    // (AudioCommand::TYPE_TTS, ko, 1회). Pi가 오프라인이면 그냥 유실된다.
    void publishAudioAnnouncement(const QString &text);
    void retryCommunication();
    // HOME_PRE_DOCK Nav2 goal 성공 직후 호출 — /medirun/vision/dock_request(true)를
    // publish해 로봇 쪽 docking_manager(ArUco 전면주차 상태머신)를 깨운다.
    // mission_manager.cpp가 RETURNING_HOME 도착 시 쓰는 것과 동일한 토픽/계약.
    void requestDocking();
    // 진행 중인 전면주차를 중단시킨다 — /medirun/vision/dock_request(false).
    void cancelDocking();

signals:
    void dispatchRequested(const QString &buttonId,
                           const QString &requestSource,
                           const QString &missionId,
                           const QDateTime &requestedAt);
    void missionHeartbeat(const QString &source, const QDateTime &receivedAt);
    void communicationRetryFinished(bool success, const QString &detail);
    // /medirun/vision/dock_result 수신 결과. success=true면 PARKED, false면
    // 실패(마커 탐색/정렬 타임아웃 등) — detail에 실패 사유가 담긴다.
    void dockResultReceived(bool success, const QString &detail);
    void logEvent(const QString &level, const QString &message);

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

#endif // MISSIONROSINTERFACE_H
