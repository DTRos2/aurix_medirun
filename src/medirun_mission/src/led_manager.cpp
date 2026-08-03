#include <map>
#include <memory>
#include <string>

#include "medirun_mission/json_utils.hpp"
#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/string.hpp"

namespace
{

class LedManager : public rclcpp::Node
{
public:
  LedManager()
  : Node("led_manager")
  {
    led_by_state_ = {
      {"IDLE", "LED:IDLE"},
      {"DISPATCHED", "LED:RUN"},
      {"NAVIGATING", "LED:RUN"},
      {"ARRIVED", "LED:ARRIVE"},
      {"PAYLOAD_REMOVED", "LED:ARRIVE"},
      {"RETURNING_HOME", "LED:RUN"},
      {"DOCKING", "LED:RUN"},
      {"DOCKED", "LED:DOCKED"},
      {"COMPLETED", "LED:IDLE"},
      {"FAILED", "LED:ERROR"},
    };

    pub_ = create_publisher<std_msgs::msg::String>("/medirun/sensor/led_command", 10);
    state_sub_ = create_subscription<std_msgs::msg::String>(
      "/medirun/mission/state", 10,
      [this](const std_msgs::msg::String::SharedPtr msg) {
        const auto state = medirun_mission::get_json_string(msg->data, "state").value_or("IDLE");
        std_msgs::msg::String out;
        out.data = led_by_state_.count(state) > 0 ? led_by_state_.at(state) : "LED:ERROR";
        pub_->publish(out);
      });
    reject_sub_ = create_subscription<std_msgs::msg::String>(
      "/medirun/mission/dispatch_rejection", 10,
      [this](const std_msgs::msg::String::SharedPtr) {
        std_msgs::msg::String out;
        out.data = "LED:STOP";
        pub_->publish(out);
      });
  }

private:
  std::map<std::string, std::string> led_by_state_;
  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr pub_;
  rclcpp::Subscription<std_msgs::msg::String>::SharedPtr state_sub_;
  rclcpp::Subscription<std_msgs::msg::String>::SharedPtr reject_sub_;
};

}  // namespace

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<LedManager>());
  rclcpp::shutdown();
  return 0;
}
