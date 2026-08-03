#include <memory>
#include <string>

#include "medirun_mission/json_utils.hpp"
#include "medirun_mission/waypoint_store.hpp"
#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/string.hpp"

namespace
{

class WaypointManager : public rclcpp::Node
{
public:
  WaypointManager()
  : Node("waypoint_manager")
  {
    declare_parameter("waypoints_file", "src/medirun_mission/config/waypoints.yaml");
    dispatch_pub_ = create_publisher<std_msgs::msg::String>("/medirun/mission/dispatch_command", 10);
    routes_ = medirun_mission::load_waypoints(get_parameter("waypoints_file").as_string());
    dispatch_sub_ = create_subscription<std_msgs::msg::String>(
      "/medirun/mission/dispatch_request", 10,
      [this](const std_msgs::msg::String::SharedPtr msg) { handle_dispatch_request(msg->data); });
    RCLCPP_INFO(get_logger(), "waypoint_manager loaded %zu routes", routes_.size());
  }

private:
  void handle_dispatch_request(const std::string & json)
  {
    const auto button_id = medirun_mission::get_json_string(json, "button_id").value_or("");
    const auto route_it = routes_.find(button_id);
    if (route_it == routes_.end()) {
      RCLCPP_WARN(get_logger(), "unknown button_id=%s", button_id.c_str());
      return;
    }

    const auto & route = route_it->second;
    std_msgs::msg::String out;
    out.data = medirun_mission::make_json_object({
      {"mission_id", medirun_mission::get_json_string(json, "mission_id").value_or(""), false},
      {"button_id", button_id, false},
      {"request_source", medirun_mission::get_json_string(json, "request_source").value_or(""), false},
      {"requested_at_sec", std::to_string(
          medirun_mission::get_json_number(json, "requested_at_sec").value_or(0.0)), true},
      {"target_waypoint_id", route.target_waypoint_id, false},
      {"frame_id", route.frame_id, false},
      {"pose_x", std::to_string(route.pose_x), true},
      {"pose_y", std::to_string(route.pose_y), true},
      {"pose_yaw", std::to_string(route.pose_yaw), true},
    });
    dispatch_pub_->publish(out);
  }

  std::map<std::string, medirun_mission::Route> routes_;
  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr dispatch_pub_;
  rclcpp::Subscription<std_msgs::msg::String>::SharedPtr dispatch_sub_;
};

}  // namespace

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<WaypointManager>());
  rclcpp::shutdown();
  return 0;
}
