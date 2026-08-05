#ifndef DBOPERATIONSOVERVIEWSERVICE_H
#define DBOPERATIONSOVERVIEWSERVICE_H

#include "operationsoverviewservice.h"

// PostgreSQL robot/robot_session/mission/system_event/connection_status_log
// 테이블(db/schema/postgresql_schema.sql) 기반 OperationsOverviewService.
// 연결은 dbconnection.h(openDbConnection)를 통해 config/db.yaml 설정으로 연다.
class DbOperationsOverviewService : public OperationsOverviewService
{
public:
    bool fetchSummary(OperationsSummaryMetrics &metrics, QString &errorMessage) override;
    bool fetchRobotStatus(QVector<RobotStatusRow> &rows, QString &errorMessage) override;
    bool fetchSystemHealth(QVector<SystemHealthRow> &rows, QString &errorMessage) override;
    bool fetchRecentEvents(int limit, QVector<OperationsEventRow> &rows, QString &errorMessage) override;

    bool recordConnectionStatus(
        int robotId, const QString &target, const QString &status, const QString &detail, QString &errorMessage) override;
    bool recordSystemEvent(
        const QString &eventType, const QString &severity, const QString &message, QString &errorMessage) override;

    bool startRobotSession(
        int robotId,
        const QString &mode,
        const QString &sessionName,
        const QVector<NavGoalPoint> &goals,
        bool hasStartPose,
        const NavGoalPoint &startPose,
        int &sessionId,
        QString &errorMessage) override;
    bool markGoalReached(int sessionId, int reachedIndex, QString &errorMessage) override;
    bool finishRobotSession(int sessionId, bool success, bool cancelled, QString &errorMessage) override;

    bool recordUserAction(
        int robotId,
        int sessionId,
        int userId,
        const QString &actionType,
        const QString &payloadJson,
        const QString &issuedBy,
        QString &errorMessage) override;
};

#endif // DBOPERATIONSOVERVIEWSERVICE_H
