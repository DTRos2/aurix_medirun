#include "waypointrepository.h"

#include "waypointfile.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QtMath>

namespace {

void quaternionFromYaw(double yaw, double &z, double &w)
{
    z = std::sin(yaw / 2.0);
    w = std::cos(yaw / 2.0);
}

} // namespace

WaypointRepository::WaypointRepository(QObject *parent)
    : QObject(parent)
{
    QString errorMessage;
    loadDefault(errorMessage);
}

QString WaypointRepository::frameId() const
{
    return m_frameId;
}

QString WaypointRepository::filePath() const
{
    return m_filePath;
}

WaypointMap WaypointRepository::waypoints() const
{
    return m_waypoints;
}

TrajectoryMap WaypointRepository::trajectories() const
{
    return m_trajectories;
}

QString WaypointRepository::workspaceRootPath() const
{
    QDir dir(QCoreApplication::applicationDirPath());
    for (int i = 0; i < 8; ++i) {
        if (QFileInfo::exists(dir.filePath(QStringLiteral("maps")))) {
            return dir.absolutePath();
        }
        if (!dir.cdUp()) {
            break;
        }
    }
    return QDir::currentPath();
}

bool WaypointRepository::loadDefault(QString &errorMessage)
{
    const QString path = defaultWaypointFilePath();
    if (path.isEmpty()) {
        errorMessage.clear();
        return false;
    }
    return loadFromFile(path, errorMessage);
}

bool WaypointRepository::loadFromFile(const QString &path, QString &errorMessage)
{
    QString frameId;
    WaypointMap waypoints;
    TrajectoryMap trajectories;
    if (!loadWaypointYaml(path, frameId, waypoints, trajectories, errorMessage)) {
        return false;
    }

    replaceAll(frameId, waypoints, trajectories, path);
    errorMessage.clear();
    return true;
}

bool WaypointRepository::saveToFile(const QString &path, QString &errorMessage)
{
    if (!saveWaypointYaml(path, m_frameId, m_waypoints, m_trajectories, errorMessage)) {
        return false;
    }
    m_filePath = path;
    emit repositoryChanged();
    return true;
}

void WaypointRepository::replaceAll(const QString &frameId,
                                    const WaypointMap &waypoints,
                                    const TrajectoryMap &trajectories,
                                    const QString &filePath)
{
    m_frameId = frameId.isEmpty() ? QStringLiteral("map") : frameId;
    m_waypoints = waypoints;
    m_trajectories = trajectories;
    m_filePath = filePath;
    emit repositoryChanged();
}

void WaypointRepository::upsertWaypoint(const WaypointDef &waypoint)
{
    upsertWaypoints({waypoint});
}

void WaypointRepository::upsertWaypoints(const QVector<WaypointDef> &waypoints)
{
    if (waypoints.isEmpty()) {
        return;
    }
    for (const WaypointDef &waypoint : waypoints) {
        m_waypoints.insert(waypoint.name, waypoint);
    }
    emit repositoryChanged();
}

bool WaypointRepository::waypoint(const QString &waypointId, WaypointDef &waypoint) const
{
    if (!m_waypoints.contains(waypointId)) {
        return false;
    }
    waypoint = m_waypoints.value(waypointId);
    return true;
}

QVector<WaypointDef> WaypointRepository::waypointsByKind(WaypointMarkerKind kind) const
{
    QVector<WaypointDef> result;
    for (auto it = m_waypoints.cbegin(); it != m_waypoints.cend(); ++it) {
        if (it.value().kind == kind) {
            result.append(it.value());
        }
    }
    return result;
}

bool WaypointRepository::waypointPose(const QString &waypointId, WaypointPose &pose, QString &errorMessage) const
{
    WaypointDef waypointDef;
    if (!waypoint(waypointId, waypointDef)) {
        errorMessage = QStringLiteral("waypoint_id=%1 를 찾을 수 없습니다.").arg(waypointId);
        return false;
    }

    pose = poseFromWaypoint(waypointDef, m_frameId);
    if (qFuzzyIsNull(pose.orientationW) && qFuzzyIsNull(pose.orientationZ)) {
        errorMessage = QStringLiteral("waypoint_id=%1 quaternion 값이 유효하지 않습니다.").arg(waypointId);
        return false;
    }
    errorMessage.clear();
    return true;
}

QString WaypointRepository::defaultWaypointFilePath() const
{
    const QByteArray override = qgetenv("AURIX_WAYPOINT_FILE");
    if (!override.isEmpty() && QFileInfo::exists(QString::fromLocal8Bit(override))) {
        return QString::fromLocal8Bit(override);
    }

    const QString workspaceRoot = workspaceRootPath();
    const QStringList candidates = {
        QDir(workspaceRoot).filePath(QStringLiteral("maps/waypoints/second.yaml")),
        QDir(workspaceRoot).filePath(QStringLiteral("maps/waypoints/first.yaml")),
        QDir(workspaceRoot).filePath(QStringLiteral("src/medirun_mission/config/waypoints.yaml")),
    };
    for (const QString &candidate : candidates) {
        if (QFileInfo::exists(candidate)) {
            return candidate;
        }
    }
    return QString();
}

WaypointPose WaypointRepository::poseFromWaypoint(const WaypointDef &waypoint, const QString &frameId)
{
    WaypointPose pose;
    pose.frameId = frameId.isEmpty() ? QStringLiteral("map") : frameId;
    pose.positionX = waypoint.x;
    pose.positionY = waypoint.y;
    pose.positionZ = 0.0;
    pose.yawRadians = waypoint.yaw;
    quaternionFromYaw(waypoint.yaw, pose.orientationZ, pose.orientationW);
    pose.valid = true;
    return pose;
}
