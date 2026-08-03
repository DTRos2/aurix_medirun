#include <memory>
#include <string>

#include "geometry_msgs/msg/twist.hpp"
#include "medirun_mission/json_utils.hpp"
#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/string.hpp"

namespace
{

class RemoteControlBridge : public rclcpp::Node
{
public:
  RemoteControlBridge()
  : Node("remote_control_bridge")
  {
    mission_pub_ = create_publisher<std_msgs::msg::String>("/medirun/ui/mission_command", 10);
    twist_pub_ = create_publisher<geometry_msgs::msg::Twist>("/medirun/ui/cmd_vel_raw", 10);
    command_sub_ = create_subscription<std_msgs::msg::String>(
      "/medirun/ui/command", 10,
      [this](const std_msgs::msg::String::SharedPtr msg) {
        const auto command_type = medirun_mission::get_json_string(msg->data, "type").value_or("");
        if (command_type == "mission") {
          std_msgs::msg::String out;
          out.data = medirun_mission::make_json_object(
            {{"command", medirun_mission::get_json_string(msg->data, "command").value_or(""), false}});
          mission_pub_->publish(out);
          return;
        }
        if (command_type == "drive") {
          geometry_msgs::msg::Twist twist;
          twist.linear.x = medirun_mission::get_json_number(msg->data, "linear_x").value_or(0.0);
          twist.angular.z = medirun_mission::get_json_number(msg->data, "angular_z").value_or(0.0);
          twist_pub_->publish(twist);
        }
      });
  }

private:
  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr mission_pub_;
  rclcpp::Publisher<geometry_msgs::msg::Twist>::SharedPtr twist_pub_;
  rclcpp::Subscription<std_msgs::msg::String>::SharedPtr command_sub_;
};

}  // namespace

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<RemoteControlBridge>());
  rclcpp::shutdown();
  return 0;
}
