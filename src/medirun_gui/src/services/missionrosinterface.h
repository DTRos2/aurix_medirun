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
    // 호출구역 도착 직후(armed=true, 홈 좌표 포함) Pi의 arrival_return_manager에
    // "반출 감시 시작"을 알린다. armed=false는 해제 — 운영자가 수동으로
    // 취소/복귀시켜서 Pi가 나중에 또 복귀 goal을 쏘면 안 될 때 쓴다. 이 신호
    // 이후로는 반출 감지·15초/60초/180초 타이머·복귀 goal 전송까지 전부
    // Pi가 로컬로 처리해서 Wi-Fi가 끊겨도 이어간다(2026-08-19).
    void publishArrivalNotice(bool armed,
                              const QString &missionId,
                              const QString &queueId,
                              double homeX,
                              double homeY,
                              double homeYaw,
                              const QString &frameId);

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
    // Pi의 arrival_return_manager가 /medirun/mission/pi_return_status로 방송하는
    // 진행상황 — "armed"/"payload_removed"/"returning"/"return_failed".
    // MissionController는 이걸 구독해서 화면 상태만 따라가고 명령은 안 내린다.
    void piReturnStatusReceived(const QString &phase, const QString &missionId, const QString &queueId);
    void logEvent(const QString &level, const QString &message);

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

#endif // MISSIONROSINTERFACE_H
