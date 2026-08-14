#ifndef OPERATIONSOVERVIEWSERVICE_H
#define OPERATIONSOVERVIEWSERVICE_H

#include "../models/waypointtypes.h"

#include <QDate>
#include <QDateTime>
#include <QString>
#include <QStringList>
#include <QVector>

// 운영 현황 페이지(OperationsOverviewPage)가 그리는 카드/테이블/헬스/이벤트
// 데이터를 조회하는 인터페이스. AuthService/DbAuthService와 동일한 패턴으로
// DbOperationsOverviewService가 실 구현체(db/schema/postgresql_schema.sql 기반)다.

struct OperationsSummaryMetrics
{
    int registeredRobots {0};
    int connectedRobots {0};
    int activeMissions {0};
    int activeEmergencyMissions {0};
    int todayCalls {0};
    int todayEmergencyCalls {0};
    int safetyEventsLast24h {0};
};

struct RobotStatusRow
{
    QString robotName;
    QString statusLabel;
    QString statusKind; // "cyan" | "muted" | "amber" | "red" — badgelabel.h 참고
    QString batteryText;
    QString routeText;
};

struct SystemHealthRow
{
    QString label;
    int percent {0};
    QString kind; // "green" | "amber" | "red"

    bool operator==(const SystemHealthRow &other) const
    {
        return label == other.label && percent == other.percent && kind == other.kind;
    }
};

struct OperationsEventRow
{
    QDateTime occurredAt;
    QString message;
    QString severityLabel; // INFO / WARN / ERROR / CRITICAL
    QString kind; // "muted" | "amber" | "red"
};

// ros/navigationservice.h의 NavPose를 그대로 쓰지 않고 여기 별도로 두는 이유:
// services/ 계층이 ros/ 계층을 몰라야 한다 (NavGoalLogRecorder가 변환해서 넘김).
struct NavGoalPoint
{
    double x {0.0};
    double y {0.0};
    double yaw {0.0};
};

// call_button 테이블 한 행. code가 웨이포인트 id(예: "BTN_01")와 대응한다.
struct CallButtonRow
{
    QString code;
    QString zoneName;
    double x {0.0};
    double y {0.0};
    double yaw {0.0};
};

class OperationsOverviewService
{
public:
    virtual ~OperationsOverviewService() = default;

    virtual bool fetchSummary(OperationsSummaryMetrics &metrics, QString &errorMessage) = 0;
    virtual bool fetchRobotStatus(QVector<RobotStatusRow> &rows, QString &errorMessage) = 0;
    virtual bool fetchSystemHealth(QVector<SystemHealthRow> &rows, QString &errorMessage) = 0;
    virtual bool fetchRecentEvents(int limit, QVector<OperationsEventRow> &rows, QString &errorMessage) = 0;

    // 시스템 설정 페이지의 로그 조회용. system_event에서 [startDate 00:00,
    // endDate+1일 00:00) 구간만 조회하고, keyword가 비어있지 않으면
    // event_type/message에 대소문자 무시 부분일치(ILIKE) 검색을 추가로 건다.
    virtual bool fetchSystemEvents(
        const QDate &startDate, const QDate &endDate, const QString &keyword, QVector<OperationsEventRow> &rows,
        QString &errorMessage) = 0;

    // target: "ros_bridge" | "database" | "serial" | "camera" | "unity"
    // status: "connected" | "disconnected" | "reconnecting" | "error"
    virtual bool recordConnectionStatus(
        int robotId, const QString &target, const QString &status, const QString &detail, QString &errorMessage) = 0;

    // severity: "info" | "warn" | "error" | "critical"
    virtual bool recordSystemEvent(
        const QString &eventType, const QString &severity, const QString &message, QString &errorMessage) = 0;

    // control_command_log 1행 기록 (제어 명령 감사 로그). commandSource: "qt" |
    // "unity" | "navigation" | "playback" | "safety". commandType: "drive" |
    // "stop" | "estop" | "mode_change" | "goal" | "resume". session_id가
    // NOT NULL이라, 진행 중(status='running')인 robot_session이 없으면
    // startDrivingSession과 동일하게 수동 세션을 하나 만들어 붙인다.
    virtual bool recordControlCommand(
        int robotId, const QString &commandSource, const QString &commandType, bool emergencyStopActive,
        const QString &issuedBy, QString &errorMessage) = 0;

    // robot_session 1행 + goals 각각에 대한 nav_goal_log 행(waypoint_seq_no로
    // 순번 구분)을 만든다. mode: "auto" | "manual" | "unity" | "playback" | "mapping".
    // 성공하면 sessionId에 새 robot_session.id를 채운다.
    // 각 행의 start_x/y/yaw: 첫 웨이포인트는 hasStartPose가 true일 때 startPose를
    // 쓰고(false면 NULL — 아직 AMCL 위치를 한 번도 못 받은 경우), 두 번째부터는
    // 직전 웨이포인트의 goal을 시작 위치로 근사한다.
    virtual bool startRobotSession(
        int robotId,
        const QString &mode,
        const QString &sessionName,
        const QVector<NavGoalPoint> &goals,
        bool hasStartPose,
        const NavGoalPoint &startPose,
        int &sessionId,
        QString &errorMessage) = 0;

    // 경로 주행 중 waypoint 하나에 도달했을 때 그 nav_goal_log 행만 'arrived'로 마감.
    virtual bool markGoalReached(int sessionId, int reachedIndex, QString &errorMessage) = 0;

    // 세션 종료: 아직 안 끝난 nav_goal_log 행 전부와 robot_session을 한 번에 마감.
    virtual bool finishRobotSession(int sessionId, bool success, bool cancelled, QString &errorMessage) = 0;

    // Qt GUI 사용자 조작 로그(user_action_log). action_type은 스키마 CHECK
    // 목록 중 하나(예: "waypoint_set_run"). payloadJson은 JSONB로 저장할
    // compact JSON 문자열(비어있으면 NULL). issuedBy는 표시용 로그인 아이디.
    // sessionId/userId는 0 이하면 NULL로 저장(연관 세션 없음 / 로그인 계정
    // 식별 불가 — mock 인증 등).
    virtual bool recordUserAction(
        int robotId,
        int sessionId,
        int userId,
        const QString &actionType,
        const QString &payloadJson,
        const QString &issuedBy,
        QString &errorMessage) = 0;

    // Qt GUI 웨이포인트 관리 페이지가 쓰는 웨이포인트 라이브러리(이름있는
    // waypoint + trajectory, waypoints/*.yaml과 동일 구조)를 waypoint_set
    // 테이블에서 불러온다. 로봇의 여러 저장 세트(이름=저장 시 쓴 파일명) 중
    // 가장 최근 것 하나를 고른다. 저장된 게 없으면 found=false를 채우고
    // true를 반환한다(에러 아님).
    virtual bool loadWaypointLibrary(
        int robotId, QString &frameId, WaypointMap &waypoints, TrajectoryMap &trajectories, bool &found,
        QString &errorMessage) = 0;

    // 이 로봇으로 저장된 waypoint_set.name 목록(최근 저장 순). 파일이 없는
    // 다른 PC에서 "DB에서 불러오기"로 고를 목록을 채우는 용도.
    virtual bool listWaypointLibraryNames(int robotId, QStringList &names, QString &errorMessage) = 0;

    // listWaypointLibraryNames로 고른 특정 이름의 세트를 불러온다.
    virtual bool loadWaypointLibraryByName(
        int robotId, const QString &name, QString &frameId, WaypointMap &waypoints, TrajectoryMap &trajectories,
        QString &errorMessage) = 0;

    // "파일로 저장" 버튼을 눌렀을 때, 같은 내용을 waypoint_set에도 upsert한다.
    // name: waypoint_set.name — 같은 이름으로 다시 저장하면 그 행을 덮어쓰고,
    // 다른 이름(예: 다른 파일명)으로 저장하면 새 행이 생긴다(robot_id+name UNIQUE).
    // YAML 파일을 다른 이름으로 저장하면 새 파일이 생기는 것과 동일한 동작을
    // 의도한 것 — 호출자(WaypointManagementPage)가 저장 파일의 basename을 넘긴다.
    virtual bool saveWaypointLibrary(
        int robotId, const QString &name, const QString &frameId, const WaypointMap &waypoints,
        const TrajectoryMap &trajectories, QString &errorMessage) = 0;

    // driving_session 시작. 현재 진행 중인(status='running') 가장 최근
    // robot_session을 찾아 연결하고, 없으면 mode='manual'인 녹화 전용 세션을
    // 새로 만든다. robotSessionId로 실제 연결된 robot_session.id를 돌려준다
    // (camera_session/camera_recording도 같은 세션에 묶기 위함).
    virtual bool startDrivingSession(int robotId, int &drivingSessionId, int &robotSessionId, QString &errorMessage) = 0;
    // driving_record 1행 추가 (재생용 경로 포인트).
    virtual bool appendDrivingRecord(
        int drivingSessionId, double x, double y, double yaw, double linearVel, double angularVel,
        double batteryPercent, QString &errorMessage) = 0;
    // driving_session.ended_at을 채워 녹화를 마감한다.
    virtual bool finishDrivingSession(int drivingSessionId, QString &errorMessage) = 0;

    // camera_session 시작 (녹화 시작 시 driving_session과 같은 robot_session에 묶어서).
    virtual bool startCameraSession(
        int robotSessionId, int drivingSessionId, const QString &cameraName, const QString &topicName,
        int &cameraSessionId, QString &errorMessage) = 0;
    // 녹화 중지 시 실제 인코딩된 영상의 길이/해상도/fps로 camera_recording 1행 기록.
    virtual bool recordCameraRecording(
        int robotSessionId, int cameraSessionId, int durationSec, int width, int height, double fps,
        const QString &codec, int &cameraRecordingId, QString &errorMessage) = 0;

    // files 테이블에 1행 기록 (스냅샷 사진, 녹화 영상 등). storagePath는 호출자가
    // 이미 로컬에 저장한 실제 경로를 그대로 넘긴다 (계정 경로 하드코딩 금지 —
    // CONVENTIONS.md 3.2절). fileCategory 예: "snapshot", "video".
    virtual bool recordFile(
        const QString &entityType, const QString &entityId, const QString &fileCategory, const QString &storagePath,
        const QString &originalFilename, const QString &mimeType, const QString &uploadedBy,
        QString &errorMessage) = 0;

    // 등록 해제되지 않은(deregistered_at IS NULL) call_button 전부를 읽어온다.
    // GUI 시작 시 웨이포인트 파일(second.yaml 등)이 재부팅 등으로 구버전이어도
    // 실제 등록된 콜존 좌표를 덮어쓰는 데 쓴다.
    virtual bool loadActiveCallButtons(QVector<CallButtonRow> &rows, QString &errorMessage) = 0;

    // 콜존 웨이포인트(BTN_* 등, kind=CallZone) 하나를 call_button에 upsert한다.
    // code(=waypoint id) UNIQUE라 이미 있으면 zone_name/좌표/yaw를 갱신하고,
    // 등록 해제돼 있었으면 다시 활성화(deregistered_at = NULL)한다.
    virtual bool upsertCallButton(
        const QString &code, const QString &zoneName, double x, double y, double yaw, QString &errorMessage) = 0;
};

#endif // OPERATIONSOVERVIEWSERVICE_H
