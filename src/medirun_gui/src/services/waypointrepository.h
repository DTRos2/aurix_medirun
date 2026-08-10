#ifndef WAYPOINTREPOSITORY_H
#define WAYPOINTREPOSITORY_H

#include "../models/missiontypes.h"

#include <QObject>

class WaypointRepository : public QObject
{
    Q_OBJECT

public:
    explicit WaypointRepository(QObject *parent = nullptr);

    QString frameId() const;
    QString filePath() const;
    WaypointMap waypoints() const;
    TrajectoryMap trajectories() const;
    QString workspaceRootPath() const;

    bool loadDefault(QString &errorMessage);
    bool loadFromFile(const QString &path, QString &errorMessage);
    bool saveToFile(const QString &path, QString &errorMessage);
    void replaceAll(const QString &frameId,
                    const WaypointMap &waypoints,
                    const TrajectoryMap &trajectories,
                    const QString &filePath);

    // waypoint.name을 key로 추가하거나 있으면 덮어쓴다. 파일 저장 없이 메모리
    // 상태만 갱신하고 repositoryChanged를 emit한다 — call_button DB 등 외부
    // 소스 값을 반영할 때 쓴다.
    void upsertWaypoint(const WaypointDef &waypoint);
    // upsertWaypoint를 여러 건에 대해 한 번에 적용하고 repositoryChanged도
    // 한 번만 emit한다(불필요한 재계산/UI 갱신 반복 방지).
    void upsertWaypoints(const QVector<WaypointDef> &waypoints);

    bool waypoint(const QString &waypointId, WaypointDef &waypoint) const;
    QVector<WaypointDef> waypointsByKind(WaypointMarkerKind kind) const;
    bool waypointPose(const QString &waypointId, WaypointPose &pose, QString &errorMessage) const;

signals:
    void repositoryChanged();

private:
    QString defaultWaypointFilePath() const;
    static WaypointPose poseFromWaypoint(const WaypointDef &waypoint, const QString &frameId);

    QString m_frameId {QStringLiteral("map")};
    QString m_filePath;
    WaypointMap m_waypoints;
    TrajectoryMap m_trajectories;
};

#endif // WAYPOINTREPOSITORY_H
