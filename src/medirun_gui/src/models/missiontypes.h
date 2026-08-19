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
    Arrived,
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
    // finishMission()이 최종 상태로 전환하는 순간의 returningHome 값을 그대로
    // 저장해둔 것 — returningHome 자체는 finishMission()에서 바로 false로
    // 리셋되어(다음 대기 호출이 막히지 않게) 실시간 관제 스텝퍼가 "실패가
    // 출동/주행 중 났는지, 복귀 단계에서 났는지" 구분할 방법이 없어진다.
    bool arrivedBeforeEnd {false};
    QString failureReason;
    double distanceRemainingMeters {-1.0};
    int progressPercent {-1};
    // MissionState::Arrived 상태에서만 의미 있음 — 홀센서(payloadLoadedChanged)로
    // AED 반출이 감지됐는지. 미션 시작 시 false로 리셋된다.
    bool payloadRemoved {false};
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
    case MissionState::Arrived:
        return QStringLiteral("ARRIVED");
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
