#pragma once

#include <map>
#include <string>

namespace medirun_mission
{

struct Route
{
  std::string target_waypoint_id;
  std::string frame_id;
  double pose_x{0.0};
  double pose_y{0.0};
  double pose_yaw{0.0};
};

std::map<std::string, Route> load_waypoints(const std::string & path);

}  // namespace medirun_mission
