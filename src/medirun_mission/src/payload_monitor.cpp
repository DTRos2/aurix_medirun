#include <memory>
#include <string>

#include "medirun_mission/json_utils.hpp"
#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/bool.hpp"
#include "std_msgs/msg/string.hpp"

namespace
{

class PayloadMonitor : public rclcpp::Node
{
public:
  PayloadMonitor()
  : Node("payload_monitor")
  {
    declare_parameter("payload_removed_confirm_sec", 0.5);

    event_pub_ = create_publisher<std_msgs::msg::String>("/medirun/mission/payload_event", 10);
    payload_sub_ = create_subscription<std_msgs::msg::Bool>(
      "/medirun/sensor/payload_loaded", 10,
      [this](const std_msgs::msg::Bool::SharedPtr msg) { handle_payload(msg->data); });
    state_sub_ = create_subscription<std_msgs::msg::String>(
      "/medirun/mission/state", 10,
      [this](const std_msgs::msg::String::SharedPtr msg) {
        mission_state_ = medirun_mission::get_json_string(msg->data, "state").value_or("IDLE");
      });
  }

private:
  void handle_payload(bool payload_loaded)
  {
    const auto now = now_sec();
    if (mission_state_ != "ARRIVED") {
      payload_removed_started_at_ = 0.0;
      last_payload_loaded_ = payload_loaded;
      return;
    }

    if (last_payload_loaded_ && !payload_loaded) {
      payload_removed_started_at_ = now;
    } else if (!last_payload_loaded_ && payload_loaded) {
      payload_removed_started_at_ = 0.0;
    }

    if (
      !payload_loaded &&
      payload_removed_started_at_ > 0.0 &&
      now - payload_removed_started_at_ >= get_parameter("payload_removed_confirm_sec").as_double())
    {
      std_msgs::msg::String msg;
      msg.data = medirun_mission::make_json_object({{"event", "PAYLOAD_REMOVED", false}});
      event_pub_->publish(msg);
      payload_removed_started_at_ = 0.0;
      mission_state_ = "PAYLOAD_REMOVED";
    }
    last_payload_loaded_ = payload_loaded;
  }

  double now_sec()
  {
    return static_cast<double>(get_clock()->now().nanoseconds()) / 1e9;
  }

  std::string mission_state_{"IDLE"};
  bool last_payload_loaded_{true};
  double payload_removed_started_at_{0.0};
  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr event_pub_;
  rclcpp::Subscription<std_msgs::msg::Bool>::SharedPtr payload_sub_;
  rclcpp::Subscription<std_msgs::msg::String>::SharedPtr state_sub_;
};

}  // namespace

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<PayloadMonitor>());
  rclcpp::shutdown();
  return 0;
}
