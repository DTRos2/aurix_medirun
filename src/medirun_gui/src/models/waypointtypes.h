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
    // DB 원본 행의 PK(call_button.id 또는 waypoint_set.id, kind에 따라 다름).
    // DB에서 불러왔거나 저장 직후에만 채워진다 — 아직 한 번도 저장 안 된
    // 항목은 0. trajectory 스텝을 이름 문자열이 아니라 이 id로 저장할 때 쓴다
    // (call_button과 waypoint_set은 서로 독립된 시퀀스라 이름/code 문자열
    // 매칭은 대소문자·중복 등으로 어긋나기 쉽다).
    int dbId {0};
};

using WaypointMap = QMap<QString, WaypointDef>;
using TrajectoryMap = QMap<QString, QStringList>; // name -> ordered waypoint names

#endif // WAYPOINTTYPES_H
