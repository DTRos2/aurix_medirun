#ifndef MISSIONROSINTERFACE_H
#define MISSIONROSINTERFACE_H

#include "../ros/navigationservice.h"

#include <QObject>
#include <QDateTime>
#include <QVector>

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
    // trajectoryPoses: 이 미션을 유발한 버튼에 연결된 경로(저장된 순서 그대로,
    // 비어있을 수 있음) — Pi가 복귀 시 이 경로를 먼저 타고 HOME으로 가도록
    // Wi-Fi가 아직 살아있는 이 시점에 통째로 넘겨준다(미션 시작 시 필요한
    // 웨이포인트를 Pi에 전부 전달해 이후 Wi-Fi가 끊겨도 주행 가능하게 하기 위함).
    void publishArrivalNotice(bool armed,
                              const QString &missionId,
                              const QString &queueId,
                              double homeX,
                              double homeY,
                              double homeYaw,
                              const QString &frameId,
                              const QVector<NavPose> &trajectoryPoses = {});
    // 조종 권한을 가진 창만 호출한다 — 다른(읽기 전용) 창들이 미션 진행
    // 상황(호출 접수/출동 중/현장 도착 등 스테퍼)을 실시간으로 따라 볼 수
    // 있도록 MissionSnapshot을 JSON으로 방송한다.
    void publishMissionSnapshot(const QString &json);

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
    // 같은 로봇에 medirun_gui 창이 여러 개 떠 있을 때, 이 프로세스가 실제로
    // 로봇에 명령을 내릴 수 있는 "조종 권한"을 쥐고 있는지. /medirun/gui/control_heartbeat
    // 를 통한 창 간 자체 조율(리더 선출) 결과다 — MissionRosNode 참고.
    void controlAuthorityChanged(bool isController);
    // 조종 권한을 가진 창이 publishMissionSnapshot()으로 방송한 내용 수신.
    void missionSnapshotReceived(const QString &json);

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

#endif // MISSIONROSINTERFACE_H
