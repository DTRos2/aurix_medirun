#ifndef OPERATIONSOVERVIEWSERVICE_H
#define OPERATIONSOVERVIEWSERVICE_H

#include <QDateTime>
#include <QString>
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
};

struct OperationsEventRow
{
    QDateTime occurredAt;
    QString message;
    QString severityLabel; // INFO / WARN / ERROR / CRITICAL
    QString kind; // "muted" | "amber" | "red"
};

class OperationsOverviewService
{
public:
    virtual ~OperationsOverviewService() = default;

    virtual bool fetchSummary(OperationsSummaryMetrics &metrics, QString &errorMessage) = 0;
    virtual bool fetchRobotStatus(QVector<RobotStatusRow> &rows, QString &errorMessage) = 0;
    virtual bool fetchSystemHealth(QVector<SystemHealthRow> &rows, QString &errorMessage) = 0;
    virtual bool fetchRecentEvents(int limit, QVector<OperationsEventRow> &rows, QString &errorMessage) = 0;

    // target: "ros_bridge" | "database" | "serial" | "camera" | "unity"
    // status: "connected" | "disconnected" | "reconnecting" | "error"
    virtual bool recordConnectionStatus(
        int robotId, const QString &target, const QString &status, const QString &detail, QString &errorMessage) = 0;

    // severity: "info" | "warn" | "error" | "critical"
    virtual bool recordSystemEvent(
        const QString &eventType, const QString &severity, const QString &message, QString &errorMessage) = 0;
};

#endif // OPERATIONSOVERVIEWSERVICE_H
