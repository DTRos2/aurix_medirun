#include <memory>
#include <string>

#include "medirun_mission/json_utils.hpp"
#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/bool.hpp"
#include "std_msgs/msg/string.hpp"

namespace
{

class EmergencyDispatcher : public rclcpp::Node
{
public:
  EmergencyDispatcher()
  : Node("emergency_dispatcher")
  {
    declare_parameter("battery_reject_percent", 30.0);
    declare_parameter("call_cooldown_sec", 2.0);

    dispatch_pub_ = create_publisher<std_msgs::msg::String>("/medirun/mission/dispatch_request", 10);
    reject_pub_ = create_publisher<std_msgs::msg::String>("/medirun/mission/dispatch_rejection", 10);

    button_sub_ = create_subscription<std_msgs::msg::String>(
      "/medirun/sensor/sos_button", 10,
      [this](const std_msgs::msg::String::SharedPtr msg) { handle_button(msg->data); });
    payload_sub_ = create_subscription<std_msgs::msg::Bool>(
      "/medirun/sensor/payload_loaded", 10,
      [this](const std_msgs::msg::Bool::SharedPtr msg) { payload_loaded_ = msg->data; });
    readiness_sub_ = create_subscription<std_msgs::msg::String>(
      "/medirun/system/readiness", 10,
      [this](const std_msgs::msg::String::SharedPtr msg) { ready_ = msg->data.find("READY") != std::string::npos; });
    battery_sub_ = create_subscription<std_msgs::msg::String>(
      "/medirun/system/battery", 10,
      [this](const std_msgs::msg::String::SharedPtr msg) {
        try {
          battery_percent_ = std::stod(msg->data);
        } catch (...) {
          battery_percent_ = 100.0;
        }
      });
  }

private:
  void handle_button(const std::string & button_id)
  {
    const auto now = now_sec();
    if (now - last_dispatch_at_ < get_parameter("call_cooldown_sec").as_double()) {
      reject("COOLDOWN", button_id);
      return;
    }
    if (!payload_loaded_) {
      reject("PAYLOAD_MISSING", button_id);
      return;
    }
    if (battery_percent_ < get_parameter("battery_reject_percent").as_double()) {
      reject("LOW_BATTERY", button_id);
      return;
    }
    if (!ready_) {
      reject("SYSTEM_NOT_READY", button_id);
      return;
    }

    last_dispatch_at_ = now;
    std_msgs::msg::String out;
    out.data = medirun_mission::make_json_object({
      {"mission_id", std::to_string(static_cast<long long>(now * 1000.0)) + "_" + button_id, false},
      {"button_id", button_id, false},
      {"requested_at_sec", std::to_string(now), true},
      {"request_source", "sos_button", false},
    });
    dispatch_pub_->publish(out);
    RCLCPP_INFO(get_logger(), "dispatch requested: button_id=%s", button_id.c_str());
  }

  void reject(const std::string & reason, const std::string & button_id)
  {
    std_msgs::msg::String msg;
    msg.data = medirun_mission::make_json_object({
      {"reason", reason, false},
      {"button_id", button_id, false},
    });
    reject_pub_->publish(msg);
    RCLCPP_WARN(get_logger(), "dispatch rejected: %s button_id=%s", reason.c_str(), button_id.c_str());
  }

  double now_sec()
  {
    return static_cast<double>(get_clock()->now().nanoseconds()) / 1e9;
  }

  bool ready_{true};
  bool payload_loaded_{true};
  double battery_percent_{100.0};
  double last_dispatch_at_{0.0};
  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr dispatch_pub_;
  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr reject_pub_;
  rclcpp::Subscription<std_msgs::msg::String>::SharedPtr button_sub_;
  rclcpp::Subscription<std_msgs::msg::Bool>::SharedPtr payload_sub_;
  rclcpp::Subscription<std_msgs::msg::String>::SharedPtr readiness_sub_;
  rclcpp::Subscription<std_msgs::msg::String>::SharedPtr battery_sub_;
};

}  // namespace

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<EmergencyDispatcher>());
  rclcpp::shutdown();
  return 0;
}
