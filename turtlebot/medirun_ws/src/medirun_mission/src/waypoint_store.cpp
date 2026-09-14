#include "medirun_mission/waypoint_store.hpp"

#include <fstream>
#include <stdexcept>

namespace medirun_mission
{
namespace
{

std::string trim(const std::string & value)
{
  const auto begin = value.find_first_not_of(" \t\r\n");
  if (begin == std::string::npos) {
    return "";
  }
  const auto end = value.find_last_not_of(" \t\r\n");
  return value.substr(begin, end - begin + 1);
}

std::string unquote(const std::string & value)
{
  if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
    return value.substr(1, value.size() - 2);
  }
  return value;
}

}  // namespace

std::map<std::string, Route> load_waypoints(const std::string & path)
{
  std::ifstream stream(path);
  if (!stream.is_open()) {
    throw std::runtime_error("failed to open waypoints file: " + path);
  }

  std::map<std::string, Route> routes;
  std::string current_button;
  std::string line;
  while (std::getline(stream, line)) {
    const auto trimmed = trim(line);
    if (trimmed.empty() || trimmed == "routes:" || trimmed == "pose:") {
      continue;
    }
    if (trimmed.rfind("BTN_", 0) == 0 && trimmed.back() == ':') {
      current_button = trimmed.substr(0, trimmed.size() - 1);
      routes[current_button] = Route{};
      continue;
    }
    if (current_button.empty()) {
      continue;
    }
    const auto pos = trimmed.find(':');
    if (pos == std::string::npos) {
      continue;
    }
    const auto key = trim(trimmed.substr(0, pos));
    const auto raw_value = unquote(trim(trimmed.substr(pos + 1)));
    auto & route = routes[current_button];
    if (key == "target_waypoint_id") {
      route.target_waypoint_id = raw_value;
    } else if (key == "frame_id") {
      route.frame_id = raw_value;
    } else if (key == "x") {
      route.pose_x = std::stod(raw_value);
    } else if (key == "y") {
      route.pose_y = std::stod(raw_value);
    } else if (key == "yaw") {
      route.pose_yaw = std::stod(raw_value);
    }
  }
  return routes;
}

}  // namespace medirun_mission
