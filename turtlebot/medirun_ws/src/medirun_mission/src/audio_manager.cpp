#include <map>
#include <memory>
#include <string>

#include "medirun_mission/json_utils.hpp"
#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/string.hpp"

namespace
{

class AudioManager : public rclcpp::Node
{
public:
  AudioManager()
  : Node("audio_manager")
  {
    audio_by_state_ = {
      {"IDLE", "주행준비되었습니다"},
      {"NAVIGATING", "aed_dispatch_started"},
      {"ARRIVED", "aed_has_arrived"},
      {"RETURNING_HOME", "returning_home"},
      {"FAILED", "operation_stopped"},
      {"COMPLETED", "docking_complete"},
    };

    pub_ = create_publisher<std_msgs::msg::String>("/medirun/audio/announcement", 10);
    sub_ = create_subscription<std_msgs::msg::String>(
      "/medirun/mission/state", 10,
      [this](const std_msgs::msg::String::SharedPtr msg) {
        const auto state = medirun_mission::get_json_string(msg->data, "state").value_or("IDLE");
        const auto it = audio_by_state_.find(state);
        if (it == audio_by_state_.end() || it->second == last_prompt_) {
          return;
        }
        last_prompt_ = it->second;
        std_msgs::msg::String out;
        out.data = it->second;
        pub_->publish(out);
      });
  }

private:
  std::map<std::string, std::string> audio_by_state_;
  std::string last_prompt_;
  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr pub_;
  rclcpp::Subscription<std_msgs::msg::String>::SharedPtr sub_;
};

}  // namespace

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<AudioManager>());
  rclcpp::shutdown();
  return 0;
}
