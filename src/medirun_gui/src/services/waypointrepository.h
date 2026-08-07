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
