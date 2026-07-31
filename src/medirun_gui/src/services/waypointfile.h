#ifndef WAYPOINTFILE_H
#define WAYPOINTFILE_H

#include "../models/waypointtypes.h"

// 웨이포인트 / trajectory YAML 로더-세이버.
//
// 기대하는 파일 형식 (pyqt_ws/dashboard의 waypoints.py 스키마를 따르되,
// kind를 선택 필드로 추가해 호출구역/경유점/홈 구분을 보존한다):
//
//   frame_id: map
//   waypoints:
//     BTN_01: {x: 4.2, y: 1.1, yaw: 0.0, kind: call_zone}
//     W-04:   {x: 6.0, y: -1.2, yaw: 0.0}          # kind 생략 시 waypoint
//     HOME:   {x: 0.0, y: 0.0, yaw: 4.712, kind: home}
//   trajectories:
//     patrol_a: [BTN_01, W-04, HOME]
//
// kind가 없는 파일(예: pyqt 대시보드가 저장한 파일)도 그대로 불러와진다.

bool loadWaypointYaml(const QString &path,
                      QString &frameId,
                      WaypointMap &waypoints,
                      TrajectoryMap &trajectories,
                      QString &errorMessage);

bool saveWaypointYaml(const QString &path,
                      const QString &frameId,
                      const WaypointMap &waypoints,
                      const TrajectoryMap &trajectories,
                      QString &errorMessage);

#endif // WAYPOINTFILE_H
