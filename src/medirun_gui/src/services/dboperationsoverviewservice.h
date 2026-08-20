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
    bool fetchSystemEvents(
        const QDate &startDate, const QDate &endDate, const QString &keyword, QVector<OperationsEventRow> &rows,
        QString &errorMessage) override;

    bool recordConnectionStatus(
        int robotId, const QString &target, const QString &status, const QString &detail, QString &errorMessage) override;
    bool recordSystemEvent(
        const QString &eventType, const QString &severity, const QString &message, QString &errorMessage) override;
    bool recordControlCommand(
        int robotId, const QString &commandSource, const QString &commandType, bool emergencyStopActive,
        const QString &issuedBy, QString &errorMessage) override;

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

    bool loadWaypointLibraryByName(
        int robotId, const QString &name, QString &frameId, WaypointMap &waypoints, TrajectoryMap &trajectories,
        QString &errorMessage) override;
    bool saveWaypointLibrary(
        int robotId, const QString &name, const QString &frameId, const WaypointMap &waypoints,
        const TrajectoryMap &trajectories, QString &errorMessage) override;
    bool deregisterHomeEntry(int robotId, QString &errorMessage) override;

    bool loadActiveWaypointEntries(int robotId, QVector<WaypointDef> &rows, QString &errorMessage) override;
    bool upsertWaypointEntry(
        int robotId, const QString &code, const QString &label, double x, double y, double yaw, int &waypointDbId,
        QString &errorMessage) override;
    bool deregisterWaypointEntry(int robotId, const QString &code, QString &errorMessage) override;

    bool loadActiveTrajectoryEntries(int robotId, QVector<TrajectoryRow> &rows, QString &errorMessage) override;
    bool upsertTrajectoryEntry(
        int robotId, const QString &name, const QVector<TrajectoryStep> &steps, QString &errorMessage) override;
    bool deregisterTrajectoryEntry(int robotId, const QString &name, QString &errorMessage) override;
    bool findTrajectoriesReferencing(
        int robotId, const QString &table, int dbId, QStringList &trajectoryNames, QString &errorMessage) override;
    bool loadTrajectoryPosesForButton(
        int robotId, const QString &buttonCode, QString &trajectoryName, QVector<NavGoalPoint> &poses,
        QString &errorMessage) override;

    bool startDrivingSession(int robotId, int &drivingSessionId, int &robotSessionId, QString &errorMessage) override;
    bool appendDrivingRecord(
        int drivingSessionId, double x, double y, double yaw, double linearVel, double angularVel,
        double batteryPercent, QString &errorMessage) override;
    bool finishDrivingSession(int drivingSessionId, QString &errorMessage) override;

    bool startCameraSession(
        int robotSessionId, int drivingSessionId, const QString &cameraName, const QString &topicName,
        int &cameraSessionId, QString &errorMessage) override;
    bool recordCameraRecording(
        int robotSessionId, int cameraSessionId, int durationSec, int width, int height, double fps,
        const QString &codec, int &cameraRecordingId, QString &errorMessage) override;

    bool recordFile(
        const QString &entityType, const QString &entityId, const QString &fileCategory, const QString &storagePath,
        const QString &originalFilename, const QString &mimeType, const QString &uploadedBy,
        QString &errorMessage) override;

    bool loadActiveCallButtons(QVector<CallButtonRow> &rows, QString &errorMessage) override;
    bool upsertCallButton(
        const QString &code, const QString &zoneName, double x, double y, double yaw, int &callButtonDbId,
        QString &errorMessage) override;
    bool deregisterCallButton(const QString &code, QString &errorMessage) override;
};

#endif // DBOPERATIONSOVERVIEWSERVICE_H
