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
};

#endif // DBOPERATIONSOVERVIEWSERVICE_H
