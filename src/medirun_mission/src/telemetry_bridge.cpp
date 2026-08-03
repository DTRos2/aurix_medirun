#include <map>
#include <memory>
#include <string>
#include <vector>

#include "medirun_mission/json_utils.hpp"
#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/bool.hpp"
#include "std_msgs/msg/int32.hpp"
#include "std_msgs/msg/string.hpp"

namespace
{

class TelemetryBridge : public rclcpp::Node
{
public:
  TelemetryBridge()
  : Node("telemetry_bridge")
  {
    snapshot_["mission_state"] = "IDLE";
    snapshot_["payload_loaded"] = "true";
    snapshot_["front_distance_cm"] = "0";
    snapshot_["rear_distance_cm"] = "0";
    snapshot_["safety_state"] = "CLEAR";

    pub_ = create_publisher<std_msgs::msg::String>("/medirun/ui/telemetry", 10);
    mission_sub_ = create_subscription<std_msgs::msg::String>(
      "/medirun/mission/state", 10,
      [this](const std_msgs::msg::String::SharedPtr msg) {
        snapshot_["mission_state"] =
          medirun_mission::get_json_string(msg->data, "state").value_or("IDLE");
        snapshot_["mission_id"] =
          medirun_mission::get_json_string(msg->data, "mission_id").value_or("");
        snapshot_["button_id"] =
          medirun_mission::get_json_string(msg->data, "button_id").value_or("");
        publish();
      });
    payload_sub_ = create_subscription<std_msgs::msg::Bool>(
      "/medirun/sensor/payload_loaded", 10,
      [this](const std_msgs::msg::Bool::SharedPtr msg) {
        snapshot_["payload_loaded"] = msg->data ? "true" : "false";
        publish();
      });
    front_sub_ = create_subscription<std_msgs::msg::Int32>(
      "/medirun/sensor/front_distance_cm", 10,
      [this](const std_msgs::msg::Int32::SharedPtr msg) {
        snapshot_["front_distance_cm"] = std::to_string(msg->data);
        publish();
      });
    rear_sub_ = create_subscription<std_msgs::msg::Int32>(
      "/medirun/sensor/rear_distance_cm", 10,
      [this](const std_msgs::msg::Int32::SharedPtr msg) {
        snapshot_["rear_distance_cm"] = std::to_string(msg->data);
        publish();
      });
    safety_sub_ = create_subscription<std_msgs::msg::String>(
      "/medirun/safety/state", 10,
      [this](const std_msgs::msg::String::SharedPtr msg) {
        snapshot_["safety_state"] =
          medirun_mission::get_json_string(msg->data, "state").value_or("CLEAR");
        publish();
      });
  }

private:
  void publish()
  {
    std_msgs::msg::String msg;
    std::vector<medirun_mission::JsonField> fields;
    fields.push_back({"mission_state", snapshot_["mission_state"], false});
    fields.push_back({"mission_id", snapshot_["mission_id"], false});
    fields.push_back({"button_id", snapshot_["button_id"], false});
    fields.push_back({"payload_loaded", snapshot_["payload_loaded"], true});
    fields.push_back({"front_distance_cm", snapshot_["front_distance_cm"], true});
    fields.push_back({"rear_distance_cm", snapshot_["rear_distance_cm"], true});
    fields.push_back({"safety_state", snapshot_["safety_state"], false});
    msg.data = medirun_mission::make_json_object(fields);
    pub_->publish(msg);
  }

  std::map<std::string, std::string> snapshot_;
  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr pub_;
  rclcpp::Subscription<std_msgs::msg::String>::SharedPtr mission_sub_;
  rclcpp::Subscription<std_msgs::msg::Bool>::SharedPtr payload_sub_;
  rclcpp::Subscription<std_msgs::msg::Int32>::SharedPtr front_sub_;
  rclcpp::Subscription<std_msgs::msg::Int32>::SharedPtr rear_sub_;
  rclcpp::Subscription<std_msgs::msg::String>::SharedPtr safety_sub_;
};

}  // namespace

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<TelemetryBridge>());
  rclcpp::shutdown();
  return 0;
}
