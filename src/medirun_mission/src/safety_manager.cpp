#include <memory>
#include <string>

#include "geometry_msgs/msg/twist.hpp"
#include "medirun_mission/json_utils.hpp"
#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/int32.hpp"
#include "std_msgs/msg/string.hpp"

namespace
{

class SafetyManager : public rclcpp::Node
{
public:
  SafetyManager()
  : Node("safety_manager")
  {
    declare_parameter("front_stop_distance_cm", 35);
    declare_parameter("rear_stop_distance_cm", 25);

    cmd_pub_ = create_publisher<geometry_msgs::msg::Twist>("/cmd_vel", 10);
    state_pub_ = create_publisher<std_msgs::msg::String>("/medirun/safety/state", 10);

    front_sub_ = create_subscription<std_msgs::msg::Int32>(
      "/medirun/sensor/front_distance_cm", 10,
      [this](const std_msgs::msg::Int32::SharedPtr msg) {
        front_cm_ = msg->data;
        publish_state("CLEAR");
      });
    rear_sub_ = create_subscription<std_msgs::msg::Int32>(
      "/medirun/sensor/rear_distance_cm", 10,
      [this](const std_msgs::msg::Int32::SharedPtr msg) {
        rear_cm_ = msg->data;
        publish_state("CLEAR");
      });

    auto handle_cmd = [this](const geometry_msgs::msg::Twist::SharedPtr msg) {
        bool blocked = false;
        geometry_msgs::msg::Twist safe = *msg;
        if (msg->linear.x > 0.0 && front_cm_ <= get_parameter("front_stop_distance_cm").as_int()) {
          safe.linear.x = 0.0;
          blocked = true;
        }
        if (msg->linear.x < 0.0 && rear_cm_ <= get_parameter("rear_stop_distance_cm").as_int()) {
          safe.linear.x = 0.0;
          blocked = true;
        }
        cmd_pub_->publish(safe);
        if (blocked) {
          publish_state("BLOCKED");
        }
      };

    nav_cmd_sub_ = create_subscription<geometry_msgs::msg::Twist>(
      "/medirun/navigation/cmd_vel_raw", 10, handle_cmd);
    ui_cmd_sub_ = create_subscription<geometry_msgs::msg::Twist>(
      "/medirun/ui/cmd_vel_raw", 10, handle_cmd);
  }

private:
  void publish_state(const std::string & state)
  {
    std_msgs::msg::String msg;
    msg.data = medirun_mission::make_json_object({
      {"state", state, false},
      {"front_distance_cm", std::to_string(front_cm_), true},
      {"rear_distance_cm", std::to_string(rear_cm_), true},
    });
    state_pub_->publish(msg);
  }

  int front_cm_{400};
  int rear_cm_{400};
  rclcpp::Publisher<geometry_msgs::msg::Twist>::SharedPtr cmd_pub_;
  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr state_pub_;
  rclcpp::Subscription<std_msgs::msg::Int32>::SharedPtr front_sub_;
  rclcpp::Subscription<std_msgs::msg::Int32>::SharedPtr rear_sub_;
  rclcpp::Subscription<geometry_msgs::msg::Twist>::SharedPtr nav_cmd_sub_;
  rclcpp::Subscription<geometry_msgs::msg::Twist>::SharedPtr ui_cmd_sub_;
};

}  // namespace

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<SafetyManager>());
  rclcpp::shutdown();
  return 0;
}
