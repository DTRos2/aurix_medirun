#include "dboperationsoverviewservice.h"
#include "dbconnection.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

namespace {

// 웨이포인트 관리 페이지에서 쓰는 이름(YAML 저장 형식과 동일하게)
QString waypointKindToJson(WaypointMarkerKind kind)
{
    switch (kind) {
    case WaypointMarkerKind::CallZone:
        return QStringLiteral("call_zone");
    case WaypointMarkerKind::Home:
        return QStringLiteral("home");
    case WaypointMarkerKind::Waypoint:
        break;
    }
    return QStringLiteral("waypoint");
}

WaypointMarkerKind waypointKindFromJson(const QString &text)
{
    if (text == QStringLiteral("call_zone")) {
        return WaypointMarkerKind::CallZone;
    }
    if (text == QStringLiteral("home")) {
        return WaypointMarkerKind::Home;
    }
    return WaypointMarkerKind::Waypoint;
}

QJsonObject waypointLibraryToJson(const QString &frameId, const WaypointMap &waypoints, const TrajectoryMap &trajectories)
{
    QJsonObject waypointsJson;
    for (auto it = waypoints.cbegin(); it != waypoints.cend(); ++it) {
        const WaypointDef &def = it.value();
        QJsonObject entry;
        entry.insert(QStringLiteral("x"), def.x);
        entry.insert(QStringLiteral("y"), def.y);
        entry.insert(QStringLiteral("yaw"), def.yaw);
        entry.insert(QStringLiteral("kind"), waypointKindToJson(def.kind));
        entry.insert(QStringLiteral("label"), def.label.isEmpty() ? it.key() : def.label);
        waypointsJson.insert(it.key(), entry);
    }

    QJsonObject trajectoriesJson;
    for (auto it = trajectories.cbegin(); it != trajectories.cend(); ++it) {
        QJsonArray names;
        for (const QString &name : it.value()) {
            names.append(name);
        }
        trajectoriesJson.insert(it.key(), names);
    }

    QJsonObject root;
    root.insert(QStringLiteral("frame_id"), frameId);
    root.insert(QStringLiteral("waypoints"), waypointsJson);
    root.insert(QStringLiteral("trajectories"), trajectoriesJson);
    return root;
}

void waypointLibraryFromJson(
    const QJsonObject &root, QString &frameId, WaypointMap &waypoints, TrajectoryMap &trajectories)
{
    frameId = root.value(QStringLiteral("frame_id")).toString(QStringLiteral("map"));

    const QJsonObject waypointsJson = root.value(QStringLiteral("waypoints")).toObject();
    for (auto it = waypointsJson.constBegin(); it != waypointsJson.constEnd(); ++it) {
        const QJsonObject entry = it.value().toObject();
        WaypointDef def;
        def.name = it.key();
        def.x = entry.value(QStringLiteral("x")).toDouble();
        def.y = entry.value(QStringLiteral("y")).toDouble();
        def.yaw = entry.value(QStringLiteral("yaw")).toDouble();
        def.kind = waypointKindFromJson(entry.value(QStringLiteral("kind")).toString());
        def.label = entry.value(QStringLiteral("label")).toString(it.key());
        waypoints.insert(def.name, def);
    }

    const QJsonObject trajectoriesJson = root.value(QStringLiteral("trajectories")).toObject();
    for (auto it = trajectoriesJson.constBegin(); it != trajectoriesJson.constEnd(); ++it) {
        QStringList names;
        for (const QJsonValue &value : it.value().toArray()) {
            names.append(value.toString());
        }
        trajectories.insert(it.key(), names);
    }
}

int percentFromCounts(int connected, int total)
{
    if (total <= 0) {
        return 0;
    }
    return qRound(100.0 * connected / total);
}

QString healthKindForPercent(int percent)
{
    if (percent >= 95) {
        return QStringLiteral("green");
    }
    if (percent >= 80) {
        return QStringLiteral("amber");
    }
    return QStringLiteral("red");
}

// 진행 중인 미션이 있으면 미션 상태를, 없으면 최근 robot_session 상태를 표시한다.
void statusLabelAndKind(const QString &missionStatus, const QString &sessionStatus, QString &label, QString &kind)
{
    if (!missionStatus.isEmpty()) {
        if (missionStatus == QStringLiteral("queued")) {
            label = QStringLiteral("● 대기열");
            kind = QStringLiteral("muted");
        } else if (missionStatus == QStringLiteral("replanning")) {
            label = QStringLiteral("● 재계획");
            kind = QStringLiteral("amber");
        } else if (missionStatus == QStringLiteral("dispensing")) {
            label = QStringLiteral("● 반출 중");
            kind = QStringLiteral("cyan");
        } else if (missionStatus == QStringLiteral("arrived")) {
            label = QStringLiteral("● 도착");
            kind = QStringLiteral("cyan");
        } else {
            // dispatched, navigating
            label = QStringLiteral("● 출동 중");
            kind = QStringLiteral("cyan");
        }
        return;
    }

    if (sessionStatus == QStringLiteral("running")) {
        label = QStringLiteral("● 운행 중");
        kind = QStringLiteral("cyan");
    } else if (sessionStatus == QStringLiteral("paused")) {
        label = QStringLiteral("● 일시정지");
        kind = QStringLiteral("amber");
    } else if (sessionStatus == QStringLiteral("failed") || sessionStatus == QStringLiteral("aborted")) {
        label = QStringLiteral("● 오류");
        kind = QStringLiteral("red");
    } else {
        label = QStringLiteral("● 대기");
        kind = QStringLiteral("muted");
    }
}

void severityLabelAndKind(const QString &severity, QString &label, QString &kind)
{
    if (severity == QStringLiteral("critical")) {
        label = QStringLiteral("CRITICAL");
        kind = QStringLiteral("red");
    } else if (severity == QStringLiteral("error")) {
        label = QStringLiteral("ERROR");
        kind = QStringLiteral("red");
    } else if (severity == QStringLiteral("warn")) {
        label = QStringLiteral("WARN");
        kind = QStringLiteral("amber");
    } else {
        label = QStringLiteral("INFO");
        kind = QStringLiteral("cyan");
    }
}

} // namespace

bool DbOperationsOverviewService::fetchSummary(OperationsSummaryMetrics &metrics, QString &errorMessage)
{
    if (!openDbConnection(errorMessage)) {
        return false;
    }

    QSqlQuery robotQuery(QStringLiteral(
        "SELECT "
        "  (SELECT COUNT(*) FROM robot WHERE deregistered_at IS NULL) AS registered_robots, "
        "  (SELECT COUNT(*) FROM ( "
        "     SELECT DISTINCT ON (robot_id) robot_id, status "
        "     FROM connection_status_log "
        "     WHERE target = 'ros_bridge' "
        "     ORDER BY robot_id, changed_at DESC, id DESC "
        "   ) latest "
        "   JOIN robot r ON r.id = latest.robot_id AND r.deregistered_at IS NULL "
        "   WHERE latest.status = 'connected' "
        "  ) AS connected_robots"));
    if (!robotQuery.isActive() || !robotQuery.next()) {
        errorMessage = QStringLiteral("DB 조회 실패: %1").arg(robotQuery.lastError().text());
        return false;
    }
    metrics.registeredRobots = robotQuery.value(QStringLiteral("registered_robots")).toInt();
    metrics.connectedRobots = robotQuery.value(QStringLiteral("connected_robots")).toInt();

    QSqlQuery missionQuery(QStringLiteral(
        "SELECT "
        "  COUNT(*) FILTER (WHERE status NOT IN ('completed', 'cancelled', 'failed')) AS active_missions, "
        "  COUNT(*) FILTER (WHERE status NOT IN ('completed', 'cancelled', 'failed') AND is_emergency) AS active_emergency_missions, "
        "  COUNT(*) FILTER (WHERE created_at::date = CURRENT_DATE) AS today_calls, "
        "  COUNT(*) FILTER (WHERE created_at::date = CURRENT_DATE AND is_emergency) AS today_emergency_calls "
        "FROM mission"));
    if (!missionQuery.isActive() || !missionQuery.next()) {
        errorMessage = QStringLiteral("DB 조회 실패: %1").arg(missionQuery.lastError().text());
        return false;
    }
    metrics.activeMissions = missionQuery.value(QStringLiteral("active_missions")).toInt();
    metrics.activeEmergencyMissions = missionQuery.value(QStringLiteral("active_emergency_missions")).toInt();
    metrics.todayCalls = missionQuery.value(QStringLiteral("today_calls")).toInt();
    metrics.todayEmergencyCalls = missionQuery.value(QStringLiteral("today_emergency_calls")).toInt();

    QSqlQuery eventQuery(QStringLiteral(
        "SELECT COUNT(*) FROM system_event "
        "WHERE severity IN ('warn', 'error', 'critical') AND created_at >= NOW() - INTERVAL '24 hours'"));
    if (!eventQuery.isActive() || !eventQuery.next()) {
        errorMessage = QStringLiteral("DB 조회 실패: %1").arg(eventQuery.lastError().text());
        return false;
    }
    metrics.safetyEventsLast24h = eventQuery.value(0).toInt();

    errorMessage.clear();
    return true;
}

bool DbOperationsOverviewService::fetchRobotStatus(QVector<RobotStatusRow> &rows, QString &errorMessage)
{
    rows.clear();

    if (!openDbConnection(errorMessage)) {
        return false;
    }

    QSqlQuery query(QStringLiteral(
        "SELECT "
        "  r.name AS robot_name, "
        "  sess.status AS session_status, "
        "  dr.battery_percent AS battery_percent, "
        "  cb.zone_name AS zone_name, "
        "  m.status AS mission_status "
        "FROM robot r "
        "LEFT JOIN LATERAL ( "
        "  SELECT status FROM robot_session "
        "  WHERE robot_session.robot_id = r.id "
        "  ORDER BY started_at DESC LIMIT 1 "
        ") sess ON true "
        "LEFT JOIN LATERAL ( "
        "  SELECT id FROM driving_session ds "
        "  WHERE ds.robot_id = r.id "
        "  ORDER BY ds.started_at DESC LIMIT 1 "
        ") latest_ds ON true "
        "LEFT JOIN LATERAL ( "
        "  SELECT battery_percent FROM driving_record "
        "  WHERE driving_record.driving_session_id = latest_ds.id "
        "  ORDER BY recorded_at DESC LIMIT 1 "
        ") dr ON true "
        "LEFT JOIN LATERAL ( "
        "  SELECT status, call_button_id FROM mission "
        "  WHERE mission.robot_id = r.id AND mission.status NOT IN ('completed', 'cancelled', 'failed') "
        "  ORDER BY created_at DESC LIMIT 1 "
        ") m ON true "
        "LEFT JOIN call_button cb ON cb.id = m.call_button_id "
        "WHERE r.deregistered_at IS NULL "
        "ORDER BY r.name"));
    if (!query.isActive()) {
        errorMessage = QStringLiteral("DB 조회 실패: %1").arg(query.lastError().text());
        return false;
    }

    while (query.next()) {
        RobotStatusRow row;
        row.robotName = query.value(QStringLiteral("robot_name")).toString();

        const QString missionStatus = query.value(QStringLiteral("mission_status")).toString();
        const QString sessionStatus = query.value(QStringLiteral("session_status")).toString();
        statusLabelAndKind(missionStatus, sessionStatus, row.statusLabel, row.statusKind);

        const QVariant battery = query.value(QStringLiteral("battery_percent"));
        row.batteryText = battery.isNull() ? QStringLiteral("-") : QStringLiteral("%1%").arg(qRound(battery.toDouble()));

        const QString zoneName = query.value(QStringLiteral("zone_name")).toString();
        row.routeText = zoneName.isEmpty() ? QStringLiteral("-") : QStringLiteral("목적지: %1").arg(zoneName);

        rows.push_back(row);
    }

    errorMessage.clear();
    return true;
}

bool DbOperationsOverviewService::fetchSystemHealth(QVector<SystemHealthRow> &rows, QString &errorMessage)
{
    rows.clear();

    if (!openDbConnection(errorMessage)) {
        return false;
    }

    // Nav2 연결/카메라 연결은 여기서 다루지 않는다 — connection_status_log의
    // target enum(ros_bridge/database/serial/camera/unity)에 "nav2"가 없고,
    // 아무 코드도 target='database'/'camera' 로그를 쓰지 않아 DB로는 항상
    // 0%가 나온다. 그 둘은 페이지가 NavigationService/CameraFeedProvider의
    // 실시간 시그널로 직접 채운다 (OperationsOverviewPage::applyNav2Connection
    // /applyCameraConnection).
    // 등록해제된 로봇의 예전 connection_status_log 행은 robot_id가 남아있어도
    // 더 이상 "현재 연결 상태"로 셀 이유가 없으므로, 지금 등록돼 있는(활성)
    // 로봇으로만 분모를 제한한다 (fetchSummary의 connected_robots 계산과 동일 조건).
    QSqlQuery connQuery(QStringLiteral(
        "SELECT "
        "  COUNT(*) FILTER (WHERE latest.status = 'connected') AS ros_connected, "
        "  COUNT(*) AS ros_total "
        "FROM ( "
        "  SELECT DISTINCT ON (robot_id) robot_id, status "
        "  FROM connection_status_log "
        "  WHERE target = 'ros_bridge' "
        "  ORDER BY robot_id, changed_at DESC, id DESC "
        ") latest "
        "JOIN robot r ON r.id = latest.robot_id AND r.deregistered_at IS NULL"));
    if (!connQuery.isActive() || !connQuery.next()) {
        errorMessage = QStringLiteral("DB 조회 실패: %1").arg(connQuery.lastError().text());
        return false;
    }

    const int rosPercent = percentFromCounts(
        connQuery.value(QStringLiteral("ros_connected")).toInt(), connQuery.value(QStringLiteral("ros_total")).toInt());

    rows.push_back({QStringLiteral("ROS2 통신"), rosPercent, healthKindForPercent(rosPercent)});
    // 이 지점까지 왔다는 것 자체가 방금 DB 쿼리가 성공했다는 뜻이라 100으로 둔다.
    rows.push_back({QStringLiteral("DB 동기화"), 100, QStringLiteral("green")});

    errorMessage.clear();
    return true;
}

bool DbOperationsOverviewService::fetchRecentEvents(int limit, QVector<OperationsEventRow> &rows, QString &errorMessage)
{
    rows.clear();

    if (!openDbConnection(errorMessage)) {
        return false;
    }

    // system_event만으로는 ROS 연결 끊김/재연결 같은 연결 상태 변화가 안 보인다
    // (TelemetryLogRecorder가 그건 connection_status_log에만 기록함) — 실시간
    // 관제 이벤트 스트림이 실제로 "지금 무슨 일이 있었는지" 다 보여주려면 두
    // 테이블을 시간순으로 합쳐야 한다.
    QSqlQuery query;
    query.prepare(QStringLiteral(
        "SELECT created_at, severity, message, robot_name FROM ("
        "  SELECT se.created_at AS created_at, se.severity AS severity, se.message AS message, "
        "         r.name AS robot_name "
        "  FROM system_event se "
        "  LEFT JOIN robot_session rs ON rs.id = se.session_id "
        "  LEFT JOIN robot r ON r.id = rs.robot_id "
        "  UNION ALL "
        "  SELECT csl.changed_at AS created_at, "
        "         CASE csl.status "
        "           WHEN 'connected' THEN 'info' "
        "           WHEN 'reconnecting' THEN 'warn' "
        "           ELSE 'error' "
        "         END AS severity, "
        "         (CASE csl.target "
        "            WHEN 'ros_bridge' THEN 'ROS2' "
        "            WHEN 'database' THEN 'DB' "
        "            WHEN 'serial' THEN '아두이노' "
        "            WHEN 'camera' THEN '카메라' "
        "            WHEN 'unity' THEN 'Unity' "
        "            ELSE csl.target "
        "          END || ' ' || "
        "          CASE csl.status "
        "            WHEN 'connected' THEN '연결됨' "
        "            WHEN 'disconnected' THEN '연결 끊김' "
        "            WHEN 'reconnecting' THEN '재연결 중' "
        "            ELSE '연결 오류' "
        "          END || "
        "          COALESCE(' · ' || NULLIF(csl.detail, ''), '')) AS message, "
        "         r.name AS robot_name "
        "  FROM connection_status_log csl "
        "  LEFT JOIN robot r ON r.id = csl.robot_id "
        ") combined_events "
        // DB에는 계속 다 쌓이지만(삭제 안 함), 화면에는 미션 관제 "오늘 처리
        // 이력"과 동일하게 최근 24시간 안의 것만 보여준다.
        "WHERE created_at >= NOW() - INTERVAL '24 hours' "
        "ORDER BY created_at DESC "
        "LIMIT :limit"));
    query.bindValue(QStringLiteral(":limit"), limit);
    if (!query.exec()) {
        errorMessage = QStringLiteral("DB 조회 실패: %1").arg(query.lastError().text());
        return false;
    }

    while (query.next()) {
        OperationsEventRow row;
        row.occurredAt = query.value(QStringLiteral("created_at")).toDateTime();
        severityLabelAndKind(query.value(QStringLiteral("severity")).toString(), row.severityLabel, row.kind);

        const QString robotName = query.value(QStringLiteral("robot_name")).toString();
        const QString message = query.value(QStringLiteral("message")).toString();
        row.message = robotName.isEmpty() ? message : QStringLiteral("%1 · %2").arg(message, robotName);

        rows.push_back(row);
    }

    errorMessage.clear();
    return true;
}

bool DbOperationsOverviewService::recordConnectionStatus(
    int robotId, const QString &target, const QString &status, const QString &detail, QString &errorMessage)
{
    if (!openDbConnection(errorMessage)) {
        return false;
    }

    QSqlQuery query;
    query.prepare(QStringLiteral(
        "INSERT INTO connection_status_log (robot_id, target, status, detail) "
        "VALUES (:robotId, :target, :status, :detail)"));
    query.bindValue(QStringLiteral(":robotId"), robotId);
    query.bindValue(QStringLiteral(":target"), target);
    query.bindValue(QStringLiteral(":status"), status);
    query.bindValue(QStringLiteral(":detail"), detail.isEmpty() ? QVariant() : QVariant(detail));
    if (!query.exec()) {
        errorMessage = QStringLiteral("연결 상태 기록 실패: %1").arg(query.lastError().text());
        return false;
    }

    errorMessage.clear();
    return true;
}

bool DbOperationsOverviewService::recordSystemEvent(
    const QString &eventType, const QString &severity, const QString &message, QString &errorMessage)
{
    if (!openDbConnection(errorMessage)) {
        return false;
    }

    QSqlQuery query;
    query.prepare(QStringLiteral(
        "INSERT INTO system_event (event_type, severity, message) VALUES (:eventType, :severity, :message)"));
    query.bindValue(QStringLiteral(":eventType"), eventType);
    query.bindValue(QStringLiteral(":severity"), severity);
    query.bindValue(QStringLiteral(":message"), message);
    if (!query.exec()) {
        errorMessage = QStringLiteral("이벤트 기록 실패: %1").arg(query.lastError().text());
        return false;
    }

    errorMessage.clear();
    return true;
}

bool DbOperationsOverviewService::startRobotSession(
    int robotId,
    const QString &mode,
    const QString &sessionName,
    const QVector<NavGoalPoint> &goals,
    bool hasStartPose,
    const NavGoalPoint &startPose,
    int &sessionId,
    QString &errorMessage)
{
    sessionId = -1;

    if (!openDbConnection(errorMessage)) {
        return false;
    }

    // 이전 주행이 정상 종료(finishRobotSession)되지 못하고 남아있을 수 있으므로
    // 같은 로봇의 미완료 세션/목표를 실패 처리해 정리한 뒤 새 세션을 시작한다.
    QSqlQuery staleGoalQuery;
    staleGoalQuery.prepare(QStringLiteral(
        "UPDATE nav_goal_log SET status = 'failed', finished_at = NOW() "
        "WHERE robot_id = :robotId AND status IN ('queued', 'accepted', 'navigating')"));
    staleGoalQuery.bindValue(QStringLiteral(":robotId"), robotId);
    if (!staleGoalQuery.exec()) {
        errorMessage = QStringLiteral("이전 주행 목표 정리 실패: %1").arg(staleGoalQuery.lastError().text());
        return false;
    }

    QSqlQuery staleSessionQuery;
    staleSessionQuery.prepare(QStringLiteral(
        "UPDATE robot_session SET status = 'failed', ended_at = NOW() "
        "WHERE robot_id = :robotId AND status IN ('planned', 'running', 'paused')"));
    staleSessionQuery.bindValue(QStringLiteral(":robotId"), robotId);
    if (!staleSessionQuery.exec()) {
        errorMessage = QStringLiteral("이전 주행 세션 정리 실패: %1").arg(staleSessionQuery.lastError().text());
        return false;
    }

    QSqlQuery sessionQuery;
    sessionQuery.prepare(QStringLiteral(
        "INSERT INTO robot_session (robot_id, session_name, mode, status) "
        "VALUES (:robotId, :sessionName, :mode, 'running') RETURNING id"));
    sessionQuery.bindValue(QStringLiteral(":robotId"), robotId);
    sessionQuery.bindValue(QStringLiteral(":sessionName"), sessionName);
    sessionQuery.bindValue(QStringLiteral(":mode"), mode);
    if (!sessionQuery.exec() || !sessionQuery.next()) {
        errorMessage = QStringLiteral("주행 세션 생성 실패: %1").arg(sessionQuery.lastError().text());
        return false;
    }
    const int newSessionId = sessionQuery.value(0).toInt();

    for (int i = 0; i < goals.size(); ++i) {
        const NavGoalPoint &goal = goals.at(i);
        // 첫 웨이포인트의 시작 위치는 마지막으로 알려진 AMCL 위치, 두 번째부터는
        // 직전 웨이포인트의 목적지를 시작 위치로 근사한다.
        const bool hasStart = i == 0 ? hasStartPose : true;
        const NavGoalPoint &start = i == 0 ? startPose : goals.at(i - 1);

        QSqlQuery goalQuery;
        goalQuery.prepare(QStringLiteral(
            "INSERT INTO nav_goal_log "
            "(robot_id, session_id, waypoint_seq_no, start_x, start_y, start_yaw, goal_x, goal_y, goal_yaw, status) "
            "VALUES (:robotId, :sessionId, :seqNo, :startX, :startY, :startYaw, :goalX, :goalY, :goalYaw, 'navigating')"));
        goalQuery.bindValue(QStringLiteral(":robotId"), robotId);
        goalQuery.bindValue(QStringLiteral(":sessionId"), newSessionId);
        goalQuery.bindValue(QStringLiteral(":seqNo"), i);
        goalQuery.bindValue(QStringLiteral(":startX"), hasStart ? QVariant(start.x) : QVariant());
        goalQuery.bindValue(QStringLiteral(":startY"), hasStart ? QVariant(start.y) : QVariant());
        goalQuery.bindValue(QStringLiteral(":startYaw"), hasStart ? QVariant(start.yaw) : QVariant());
        goalQuery.bindValue(QStringLiteral(":goalX"), goal.x);
        goalQuery.bindValue(QStringLiteral(":goalY"), goal.y);
        goalQuery.bindValue(QStringLiteral(":goalYaw"), goal.yaw);
        if (!goalQuery.exec()) {
            errorMessage = QStringLiteral("주행 목표 기록 실패: %1").arg(goalQuery.lastError().text());
            return false;
        }
    }

    sessionId = newSessionId;
    errorMessage.clear();
    return true;
}

bool DbOperationsOverviewService::markGoalReached(int sessionId, int reachedIndex, QString &errorMessage)
{
    if (!openDbConnection(errorMessage)) {
        return false;
    }

    QSqlQuery query;
    query.prepare(QStringLiteral(
        "UPDATE nav_goal_log SET status = 'arrived', finished_at = NOW() "
        "WHERE session_id = :sessionId AND waypoint_seq_no = :seqNo AND status = 'navigating'"));
    query.bindValue(QStringLiteral(":sessionId"), sessionId);
    query.bindValue(QStringLiteral(":seqNo"), reachedIndex);
    if (!query.exec()) {
        errorMessage = QStringLiteral("웨이포인트 도달 기록 실패: %1").arg(query.lastError().text());
        return false;
    }

    errorMessage.clear();
    return true;
}

bool DbOperationsOverviewService::finishRobotSession(int sessionId, bool success, bool cancelled, QString &errorMessage)
{
    if (!openDbConnection(errorMessage)) {
        return false;
    }

    // success/cancelled 어느 쪽도 아니면 실패(failed)로 본다.
    const QString goalStatus = success ? QStringLiteral("arrived") : cancelled ? QStringLiteral("cancelled") : QStringLiteral("failed");
    const QString sessionStatus = success ? QStringLiteral("completed") : cancelled ? QStringLiteral("aborted") : QStringLiteral("failed");

    QSqlQuery goalQuery;
    goalQuery.prepare(QStringLiteral(
        "UPDATE nav_goal_log SET status = :status, finished_at = NOW() "
        "WHERE session_id = :sessionId AND status NOT IN ('arrived', 'failed', 'cancelled')"));
    goalQuery.bindValue(QStringLiteral(":status"), goalStatus);
    goalQuery.bindValue(QStringLiteral(":sessionId"), sessionId);
    if (!goalQuery.exec()) {
        errorMessage = QStringLiteral("주행 목표 마감 실패: %1").arg(goalQuery.lastError().text());
        return false;
    }

    QSqlQuery sessionQuery;
    sessionQuery.prepare(QStringLiteral(
        "UPDATE robot_session SET status = :status, ended_at = NOW() WHERE id = :sessionId"));
    sessionQuery.bindValue(QStringLiteral(":status"), sessionStatus);
    sessionQuery.bindValue(QStringLiteral(":sessionId"), sessionId);
    if (!sessionQuery.exec()) {
        errorMessage = QStringLiteral("주행 세션 마감 실패: %1").arg(sessionQuery.lastError().text());
        return false;
    }

    errorMessage.clear();
    return true;
}

bool DbOperationsOverviewService::recordUserAction(
    int robotId,
    int sessionId,
    int userId,
    const QString &actionType,
    const QString &payloadJson,
    const QString &issuedBy,
    QString &errorMessage)
{
    if (!openDbConnection(errorMessage)) {
        return false;
    }

    QSqlQuery query;
    query.prepare(QStringLiteral(
        "INSERT INTO user_action_log (robot_id, session_id, user_id, action_type, payload, issued_by, occurred_at) "
        "VALUES (:robotId, :sessionId, :userId, :actionType, :payload::jsonb, :issuedBy, NOW())"));
    query.bindValue(QStringLiteral(":robotId"), robotId);
    query.bindValue(QStringLiteral(":sessionId"), sessionId > 0 ? QVariant(sessionId) : QVariant());
    query.bindValue(QStringLiteral(":userId"), userId > 0 ? QVariant(userId) : QVariant());
    query.bindValue(QStringLiteral(":actionType"), actionType);
    query.bindValue(QStringLiteral(":payload"), payloadJson.isEmpty() ? QVariant() : QVariant(payloadJson));
    query.bindValue(QStringLiteral(":issuedBy"), issuedBy.isEmpty() ? QVariant() : QVariant(issuedBy));
    if (!query.exec()) {
        errorMessage = QStringLiteral("사용자 조작 기록 실패: %1").arg(query.lastError().text());
        return false;
    }

    errorMessage.clear();
    return true;
}

bool DbOperationsOverviewService::loadWaypointLibrary(
    int robotId, QString &frameId, WaypointMap &waypoints, TrajectoryMap &trajectories, bool &found, QString &errorMessage)
{
    found = false;
    frameId = QStringLiteral("map");
    waypoints.clear();
    trajectories.clear();

    if (!openDbConnection(errorMessage)) {
        return false;
    }

    QSqlQuery query;
    query.prepare(QStringLiteral(
        "SELECT waypoints FROM waypoint_set WHERE robot_id = :robotId "
        "ORDER BY created_at DESC, id DESC LIMIT 1"));
    query.bindValue(QStringLiteral(":robotId"), robotId);
    if (!query.exec()) {
        errorMessage = QStringLiteral("웨이포인트 라이브러리 조회 실패: %1").arg(query.lastError().text());
        return false;
    }

    if (!query.next()) {
        errorMessage.clear();
        return true;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(query.value(0).toString().toUtf8());
    if (!doc.isObject()) {
        errorMessage = QStringLiteral("저장된 웨이포인트 라이브러리 형식이 올바르지 않습니다.");
        return false;
    }

    waypointLibraryFromJson(doc.object(), frameId, waypoints, trajectories);
    found = true;
    errorMessage.clear();
    return true;
}

bool DbOperationsOverviewService::listWaypointLibraryNames(int robotId, QStringList &names, QString &errorMessage)
{
    names.clear();

    if (!openDbConnection(errorMessage)) {
        return false;
    }

    QSqlQuery query;
    query.prepare(QStringLiteral(
        "SELECT name FROM waypoint_set WHERE robot_id = :robotId ORDER BY created_at DESC, id DESC"));
    query.bindValue(QStringLiteral(":robotId"), robotId);
    if (!query.exec()) {
        errorMessage = QStringLiteral("웨이포인트 라이브러리 목록 조회 실패: %1").arg(query.lastError().text());
        return false;
    }

    while (query.next()) {
        names.append(query.value(0).toString());
    }

    errorMessage.clear();
    return true;
}

bool DbOperationsOverviewService::loadWaypointLibraryByName(
    int robotId, const QString &name, QString &frameId, WaypointMap &waypoints, TrajectoryMap &trajectories,
    QString &errorMessage)
{
    frameId = QStringLiteral("map");
    waypoints.clear();
    trajectories.clear();

    if (!openDbConnection(errorMessage)) {
        return false;
    }

    QSqlQuery query;
    query.prepare(QStringLiteral("SELECT waypoints FROM waypoint_set WHERE robot_id = :robotId AND name = :name"));
    query.bindValue(QStringLiteral(":robotId"), robotId);
    query.bindValue(QStringLiteral(":name"), name);
    if (!query.exec()) {
        errorMessage = QStringLiteral("웨이포인트 라이브러리 조회 실패: %1").arg(query.lastError().text());
        return false;
    }

    if (!query.next()) {
        errorMessage = QStringLiteral("'%1' 웨이포인트 라이브러리를 찾을 수 없습니다.").arg(name);
        return false;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(query.value(0).toString().toUtf8());
    if (!doc.isObject()) {
        errorMessage = QStringLiteral("저장된 웨이포인트 라이브러리 형식이 올바르지 않습니다.");
        return false;
    }

    waypointLibraryFromJson(doc.object(), frameId, waypoints, trajectories);
    errorMessage.clear();
    return true;
}

bool DbOperationsOverviewService::saveWaypointLibrary(
    int robotId, const QString &name, const QString &frameId, const WaypointMap &waypoints,
    const TrajectoryMap &trajectories, QString &errorMessage)
{
    if (!openDbConnection(errorMessage)) {
        return false;
    }

    const QJsonObject root = waypointLibraryToJson(frameId, waypoints, trajectories);
    const QString json = QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Compact));

    QSqlQuery query;
    query.prepare(QStringLiteral(
        "INSERT INTO waypoint_set (robot_id, name, waypoints) VALUES (:robotId, :name, :waypoints::jsonb) "
        "ON CONFLICT (robot_id, name) DO UPDATE SET waypoints = EXCLUDED.waypoints"));
    query.bindValue(QStringLiteral(":robotId"), robotId);
    query.bindValue(QStringLiteral(":name"), name);
    query.bindValue(QStringLiteral(":waypoints"), json);
    if (!query.exec()) {
        errorMessage = QStringLiteral("웨이포인트 라이브러리 저장 실패: %1").arg(query.lastError().text());
        return false;
    }

    errorMessage.clear();
    return true;
}
