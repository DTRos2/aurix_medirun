#ifndef WAYPOINTTYPES_H
#define WAYPOINTTYPES_H

#include <QMap>
#include <QPointF>
#include <QString>
#include <QStringList>

enum class WaypointMarkerKind {
    CallZone,
    Waypoint,
    Home
};

struct WaypointMapMarker
{
    QString id;
    QString label;
    QPointF normalizedPos;
    WaypointMarkerKind kind {WaypointMarkerKind::Waypoint};
    // radians, world/map frame (WaypointDef::yaw 그대로) — 지도에 방향 화살표로
    // 표시해 사용자가 이 웨이포인트에서 로봇이 어느 쪽을 보게 되는지 알 수 있게 한다.
    double yaw {0.0};
};

// A waypoint definition in map (world) coordinates. x/y are meters in the
// map frame, yaw is radians. Mirrors the waypoints.py schema used by the
// reference pyqt dashboard (frame_id + waypoints + trajectories), extended
// with an optional `kind` so this GUI's call-zone / waypoint / home
// distinction survives a save/load round trip.
struct WaypointDef
{
    QString name;   // YAML key / stable id, e.g. "BTN_01", "W-04", "HOME"
    QString label;  // human-readable display name, e.g. "중앙 대합실"
    double x {0.0};
    double y {0.0};
    double yaw {0.0};
    WaypointMarkerKind kind {WaypointMarkerKind::Waypoint};
};

using WaypointMap = QMap<QString, WaypointDef>;
using TrajectoryMap = QMap<QString, QStringList>; // name -> ordered waypoint names

#endif // WAYPOINTTYPES_H
