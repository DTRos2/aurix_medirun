#include <chrono>
#include <memory>
#include <string>

#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/string.hpp"

namespace
{

class ReadyNotifier : public rclcpp::Node
{
public:
  ReadyNotifier()
  : Node("ready_notifier")
  {
    declare_parameter("ready_announcement", "주행준비되었습니다");
    declare_parameter("ready_led_command", "LED:IDLE");
    declare_parameter("startup_delay_ms", 1200);

    led_pub_ = create_publisher<std_msgs::msg::String>("/medirun/sensor/led_command", 10);
    led_arduino_pub_ = create_publisher<std_msgs::msg::String>("/arduino/led", 10);
    announcement_pub_ =
      create_publisher<std_msgs::msg::String>("/medirun/audio/announcement", 10);

    const auto delay_ms = get_parameter("startup_delay_ms").as_int();
    timer_ = create_wall_timer(
      std::chrono::milliseconds(delay_ms),
      [this]() {
        publish_ready_state();
        timer_->cancel();
      });
  }

private:
  void publish_ready_state()
  {
    std_msgs::msg::String led_msg;
    led_msg.data = get_parameter("ready_led_command").as_string();
    led_pub_->publish(led_msg);
    led_arduino_pub_->publish(led_msg);

    std_msgs::msg::String announcement_msg;
    announcement_msg.data = get_parameter("ready_announcement").as_string();
    announcement_pub_->publish(announcement_msg);

    RCLCPP_INFO(
      get_logger(), "published ready announcement and green LED command");
  }

  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr led_pub_;
  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr led_arduino_pub_;
  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr announcement_pub_;
  rclcpp::TimerBase::SharedPtr timer_;
};

}  // namespace

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<ReadyNotifier>());
  rclcpp::shutdown();
  return 0;
}
