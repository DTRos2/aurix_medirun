#ifndef MISSIONTYPES_H
#define MISSIONTYPES_H

#include "waypointtypes.h"

#include <QDateTime>
#include <QString>
#include <QVector>

enum class MissionQueueStatus {
    Waiting,
    Active,
    Completed,
    Cancelled,
    Diverted,
    Failed
};

enum class MissionState {
    Idle,
    Starting,
    Navigating,
    Cancelling,
    Diverting,
    ReturningHome,
    Completed,
    Cancelled,
    Failed
};

struct WaypointPose
{
    QString frameId {QStringLiteral("map")};
    double positionX {0.0};
    double positionY {0.0};
    double positionZ {0.0};
    double orientationX {0.0};
    double orientationY {0.0};
    double orientationZ {0.0};
    double orientationW {1.0};
    double yawRadians {0.0};
    bool valid {false};
};

struct MissionQueueItem
{
    QString queueId;
    QString waypointId;
    QString waypointName;
    QDateTime requestedAt;
    int enqueueOrder {0};
    MissionQueueStatus status {MissionQueueStatus::Waiting};
    QString requestSource;
    QString missionId;
    WaypointPose pose;
};

struct MissionHistoryEntry
{
    QString historyId;
    QDateTime occurredAt;
    QString eventType;
    QString waypointId;
    QString waypointName;
    QString missionId;
    QString queueId;
    QString result;
    QString detailMessage;
};

struct MissionSnapshot
{
    QString missionId;
    QString queueId;
    QString originName;
    QString destinationName;
    QString currentWaypointName;
    QString nextWaypointName;
    MissionState state {MissionState::Idle};
    QDateTime startedAt;
    qint64 elapsedSeconds {0};
    int waitingCount {0};
    bool returningHome {false};
    QString failureReason;
    double distanceRemainingMeters {-1.0};
    int progressPercent {-1};
};

inline QString missionQueueStatusText(MissionQueueStatus status)
{
    switch (status) {
    case MissionQueueStatus::Waiting:
        return QStringLiteral("WAITING");
    case MissionQueueStatus::Active:
        return QStringLiteral("ACTIVE");
    case MissionQueueStatus::Completed:
        return QStringLiteral("COMPLETED");
    case MissionQueueStatus::Cancelled:
        return QStringLiteral("CANCELLED");
    case MissionQueueStatus::Diverted:
        return QStringLiteral("DIVERTED");
    case MissionQueueStatus::Failed:
        return QStringLiteral("FAILED");
    }
    return QStringLiteral("UNKNOWN");
}

inline QString missionStateText(MissionState state)
{
    switch (state) {
    case MissionState::Idle:
        return QStringLiteral("IDLE");
    case MissionState::Starting:
        return QStringLiteral("STARTING");
    case MissionState::Navigating:
        return QStringLiteral("NAVIGATING");
    case MissionState::Cancelling:
        return QStringLiteral("CANCELLING");
    case MissionState::Diverting:
        return QStringLiteral("DIVERTING");
    case MissionState::ReturningHome:
        return QStringLiteral("RETURNING_HOME");
    case MissionState::Completed:
        return QStringLiteral("COMPLETED");
    case MissionState::Cancelled:
        return QStringLiteral("CANCELLED");
    case MissionState::Failed:
        return QStringLiteral("FAILED");
    }
    return QStringLiteral("UNKNOWN");
}

#endif // MISSIONTYPES_H
