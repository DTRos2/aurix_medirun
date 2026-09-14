#include <memory>
#include <string>

#include "medirun_mission/json_utils.hpp"
#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/bool.hpp"
#include "std_msgs/msg/string.hpp"

namespace
{

struct MissionContext
{
  std::string mission_id;
  std::string button_id;
  std::string target_waypoint_id;
};

class MissionManager : public rclcpp::Node
{
public:
  MissionManager()
  : Node("mission_manager")
  {
    declare_parameter("return_delay_after_payload_removed_sec", 15.0);
    declare_parameter("arrival_timeout_sec", 180.0);
    declare_parameter("home_waypoint_id", "HOME");
    declare_parameter("docking_waypoint_id", "DOCK");
    declare_parameter("simulate_navigation", true);

    nav_goal_pub_ = create_publisher<std_msgs::msg::String>("/medirun/navigation/goal", 10);
    nav_cancel_pub_ = create_publisher<std_msgs::msg::Bool>("/medirun/navigation/cancel", 10);
    state_pub_ = create_publisher<std_msgs::msg::String>("/medirun/mission/state", 10);
    dock_pub_ = create_publisher<std_msgs::msg::Bool>("/medirun/vision/dock_request", 10);

    dispatch_sub_ = create_subscription<std_msgs::msg::String>(
      "/medirun/mission/dispatch_command", 10,
      [this](const std_msgs::msg::String::SharedPtr msg) { start_mission(msg->data); });
    nav_status_sub_ = create_subscription<std_msgs::msg::String>(
      "/medirun/navigation/status", 10,
      [this](const std_msgs::msg::String::SharedPtr msg) { handle_navigation_status(msg->data); });
    payload_event_sub_ = create_subscription<std_msgs::msg::String>(
      "/medirun/mission/payload_event", 10,
      [this](const std_msgs::msg::String::SharedPtr msg) { handle_payload_event(msg->data); });
    dock_result_sub_ = create_subscription<std_msgs::msg::String>(
      "/medirun/vision/dock_result", 10,
      [this](const std_msgs::msg::String::SharedPtr msg) { handle_dock_result(msg->data); });
    ui_command_sub_ = create_subscription<std_msgs::msg::String>(
      "/medirun/ui/mission_command", 10,
      [this](const std_msgs::msg::String::SharedPtr msg) { handle_operator_command(msg->data); });

    timer_ = create_wall_timer(std::chrono::milliseconds(500), [this]() { check_deadline(); });
    publish_state();
  }

private:
  void start_mission(const std::string & json)
  {
    if (state_ != "IDLE" && state_ != "COMPLETED" && state_ != "FAILED") {
      RCLCPP_WARN(get_logger(), "new dispatch ignored while state=%s", state_.c_str());
      return;
    }
    mission_.mission_id = medirun_mission::get_json_string(json, "mission_id").value_or("");
    mission_.button_id = medirun_mission::get_json_string(json, "button_id").value_or("");
    mission_.target_waypoint_id =
      medirun_mission::get_json_string(json, "target_waypoint_id").value_or("");
    transition("DISPATCHED");
    send_nav_goal(mission_.target_waypoint_id, "service_target");
    transition("NAVIGATING");
  }

  void handle_navigation_status(const std::string & json)
  {
    const auto status = medirun_mission::get_json_string(json, "status").value_or("");
    const auto goal_type = medirun_mission::get_json_string(json, "goal_type").value_or("");
    if (status != "SUCCEEDED") {
      if (status == "FAILED") {
        transition("FAILED");
      }
      return;
    }
    if (goal_type == "service_target" && state_ == "NAVIGATING") {
      transition("ARRIVED");
      deadline_sec_ = now_sec() + get_parameter("arrival_timeout_sec").as_double();
      return;
    }
    if (goal_type == "return_home" && state_ == "RETURNING_HOME") {
      transition("DOCKING");
      std_msgs::msg::Bool dock_msg;
      dock_msg.data = true;
      dock_pub_->publish(dock_msg);
    }
  }

  void handle_payload_event(const std::string & json)
  {
    const auto event = medirun_mission::get_json_string(json, "event").value_or("");
    if (event == "PAYLOAD_REMOVED" && state_ == "ARRIVED") {
      transition("PAYLOAD_REMOVED");
      deadline_sec_ = now_sec() + get_parameter("return_delay_after_payload_removed_sec").as_double();
    }
  }

  void handle_dock_result(const std::string & json)
  {
    if (state_ != "DOCKING") {
      return;
    }
    const auto status = medirun_mission::get_json_string(json, "status").value_or("");
    if (status == "SUCCEEDED") {
      transition("DOCKED");
      transition("COMPLETED");
    } else if (status == "FAILED") {
      transition("FAILED");
    }
  }

  void handle_operator_command(const std::string & json)
  {
    const auto command = medirun_mission::get_json_string(json, "command").value_or("");
    if (command == "cancel") {
      std_msgs::msg::Bool cancel;
      cancel.data = true;
      nav_cancel_pub_->publish(cancel);
      transition("FAILED");
    } else if (command == "return_home") {
      begin_return_home();
    }
  }

  void check_deadline()
  {
    if ((state_ != "ARRIVED" && state_ != "PAYLOAD_REMOVED") || deadline_sec_ <= 0.0) {
      return;
    }
    if (now_sec() < deadline_sec_) {
      return;
    }
    begin_return_home();
  }

  void begin_return_home()
  {
    if (state_ == "RETURNING_HOME" || state_ == "DOCKING" || state_ == "DOCKED" || state_ == "COMPLETED") {
      return;
    }
    transition("RETURNING_HOME");
    send_nav_goal(get_parameter("home_waypoint_id").as_string(), "return_home");
  }

  void send_nav_goal(const std::string & waypoint_id, const std::string & goal_type)
  {
    std_msgs::msg::String msg;
    msg.data = medirun_mission::make_json_object({
      {"mission_id", mission_.mission_id, false},
      {"goal_type", goal_type, false},
      {"waypoint_id", waypoint_id, false},
    });
    nav_goal_pub_->publish(msg);
  }

  void transition(const std::string & next_state)
  {
    state_ = next_state;
    publish_state();
    RCLCPP_INFO(get_logger(), "mission transition -> %s", state_.c_str());
  }

  void publish_state()
  {
    std_msgs::msg::String msg;
    msg.data = medirun_mission::make_json_object({
      {"mission_id", mission_.mission_id, false},
      {"state", state_, false},
      {"button_id", mission_.button_id, false},
      {"target_waypoint_id", mission_.target_waypoint_id, false},
    });
    state_pub_->publish(msg);
  }

  double now_sec()
  {
    return static_cast<double>(get_clock()->now().nanoseconds()) / 1e9;
  }

  MissionContext mission_;
  std::string state_{"IDLE"};
  double deadline_sec_{0.0};
  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr nav_goal_pub_;
  rclcpp::Publisher<std_msgs::msg::Bool>::SharedPtr nav_cancel_pub_;
  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr state_pub_;
  rclcpp::Publisher<std_msgs::msg::Bool>::SharedPtr dock_pub_;
  rclcpp::Subscription<std_msgs::msg::String>::SharedPtr dispatch_sub_;
  rclcpp::Subscription<std_msgs::msg::String>::SharedPtr nav_status_sub_;
  rclcpp::Subscription<std_msgs::msg::String>::SharedPtr payload_event_sub_;
  rclcpp::Subscription<std_msgs::msg::String>::SharedPtr dock_result_sub_;
  rclcpp::Subscription<std_msgs::msg::String>::SharedPtr ui_command_sub_;
  rclcpp::TimerBase::SharedPtr timer_;
};

}  // namespace

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<MissionManager>());
  rclcpp::shutdown();
  return 0;
}
