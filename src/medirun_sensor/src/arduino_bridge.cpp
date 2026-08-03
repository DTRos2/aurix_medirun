#include "medirun_sensor/serial_utils.hpp"

#include <memory>
#include <sstream>
#include <string>

#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/bool.hpp"
#include "std_msgs/msg/int32.hpp"
#include "std_msgs/msg/string.hpp"

namespace
{

std::string json_escape(const std::string & value)
{
  std::string escaped;
  escaped.reserve(value.size());
  for (const char c : value) {
    switch (c) {
      case '\\':
        escaped += "\\\\";
        break;
      case '"':
        escaped += "\\\"";
        break;
      default:
        escaped += c;
        break;
    }
  }
  return escaped;
}

class ArduinoBridge : public rclcpp::Node
{
public:
  ArduinoBridge()
  : Node("arduino_bridge")
  {
    declare_parameter("serial_port", "/dev/ttyACM0");
    declare_parameter("baud_rate", 9600);
    declare_parameter("mock_mode", true);
    declare_parameter("poll_interval_sec", 0.05);
    declare_parameter("mock_front_distance_cm", 120);
    declare_parameter("mock_rear_distance_cm", 90);
    declare_parameter("mock_payload_loaded", true);

    mock_mode_ = get_parameter("mock_mode").as_bool();
    last_led_command_ = "LED:IDLE";

    front_pub_ = create_publisher<std_msgs::msg::Int32>("/medirun/sensor/front_distance_cm", 10);
    front_arduino_pub_ = create_publisher<std_msgs::msg::Int32>("/arduino/front_distance_cm", 10);
    rear_pub_ = create_publisher<std_msgs::msg::Int32>("/medirun/sensor/rear_distance_cm", 10);
    rear_arduino_pub_ = create_publisher<std_msgs::msg::Int32>("/arduino/rear_distance_cm", 10);
    payload_pub_ = create_publisher<std_msgs::msg::Bool>("/medirun/sensor/payload_loaded", 10);
    payload_arduino_pub_ = create_publisher<std_msgs::msg::Bool>("/arduino/payload_loaded", 10);
    sos_pub_ = create_publisher<std_msgs::msg::String>("/medirun/sensor/sos_button", 10);
    button_arduino_pub_ = create_publisher<std_msgs::msg::String>("/arduino/button", 10);
    diag_pub_ = create_publisher<std_msgs::msg::String>("/medirun/sensor/arduino_status", 10);
    diag_arduino_pub_ = create_publisher<std_msgs::msg::String>("/arduino/status", 10);

    led_sub_ = create_subscription<std_msgs::msg::String>(
      "/medirun/sensor/led_command", 10,
      [this](const std_msgs::msg::String::SharedPtr msg) { handle_led_command(msg->data); });
    led_arduino_sub_ = create_subscription<std_msgs::msg::String>(
      "/arduino/led", 10,
      [this](const std_msgs::msg::String::SharedPtr msg) {
        handle_led_command(msg->data);
      });

    if (!mock_mode_) {
      medirun_sensor::SerialConfig config;
      config.port = get_parameter("serial_port").as_string();
      config.baud_rate = get_parameter("baud_rate").as_int();
      reader_ = std::make_unique<medirun_sensor::SerialLineReader>(config);
      if (!reader_->open()) {
        RCLCPP_WARN(get_logger(), "serial unavailable, forcing mock mode");
        reader_.reset();
        mock_mode_ = true;
      }
    }

    const auto interval = std::chrono::duration<double>(get_parameter("poll_interval_sec").as_double());
    timer_ = create_wall_timer(
      std::chrono::duration_cast<std::chrono::milliseconds>(interval),
      [this]() { poll_serial(); });

    RCLCPP_INFO(get_logger(), "arduino_bridge ready: mock_mode=%s", mock_mode_ ? "true" : "false");
  }

private:
  void poll_serial()
  {
    if (mock_mode_) {
      publish_mock_state();
      return;
    }
    if (!reader_) {
      return;
    }
    const auto line = reader_->read_line();
    if (!line.has_value()) {
      return;
    }
    if (*line == "READY") {
      publish_diag("READY", "source", "robot_arduino");
      return;
    }
    const auto [key, value] = medirun_sensor::parse_key_value_line(*line);
    if (key == "DIST") {
      publish_int(front_pub_, front_arduino_pub_, std::stoi(value));
    } else if (key == "DIST_REAR") {
      publish_int(rear_pub_, rear_arduino_pub_, std::stoi(value));
    } else if (key == "HALL") {
      publish_bool(payload_pub_, payload_arduino_pub_, value == "1");
    } else if (key.rfind("BTN_", 0) == 0) {
      publish_button(key);
    } else if (key == "RAW") {
      publish_diag("RAW_HALL", "value", value);
    } else {
      publish_diag("IGNORED", "line", *line);
    }
  }

  void publish_mock_state()
  {
    publish_int(front_pub_, front_arduino_pub_, get_parameter("mock_front_distance_cm").as_int());
    publish_int(rear_pub_, rear_arduino_pub_, get_parameter("mock_rear_distance_cm").as_int());
    publish_bool(payload_pub_, payload_arduino_pub_, get_parameter("mock_payload_loaded").as_bool());
  }

  void handle_led_command(const std::string & command)
  {
    last_led_command_ = command;
    if (reader_) {
      reader_->write_line(last_led_command_);
    }
    publish_diag("LED_COMMAND", "command", last_led_command_);
  }

  void publish_diag(const std::string & state, const std::string & key, const std::string & value)
  {
    std_msgs::msg::String msg;
    std::ostringstream out;
    out << "{\"state\":\"" << json_escape(state) << "\",\"" << json_escape(key) << "\":\""
        << json_escape(value) << "\"}";
    msg.data = out.str();
    diag_pub_->publish(msg);
    diag_arduino_pub_->publish(msg);
  }

  void publish_button(const std::string & button_id)
  {
    std_msgs::msg::String msg;
    msg.data = button_id;
    sos_pub_->publish(msg);
    button_arduino_pub_->publish(msg);
    publish_diag("BUTTON", "button_id", button_id);
  }

  static void publish_int(
    const rclcpp::Publisher<std_msgs::msg::Int32>::SharedPtr & publisher,
    const rclcpp::Publisher<std_msgs::msg::Int32>::SharedPtr & alias_publisher,
    int value)
  {
    std_msgs::msg::Int32 msg;
    msg.data = value;
    publisher->publish(msg);
    alias_publisher->publish(msg);
  }

  static void publish_bool(
    const rclcpp::Publisher<std_msgs::msg::Bool>::SharedPtr & publisher,
    const rclcpp::Publisher<std_msgs::msg::Bool>::SharedPtr & alias_publisher,
    bool value)
  {
    std_msgs::msg::Bool msg;
    msg.data = value;
    publisher->publish(msg);
    alias_publisher->publish(msg);
  }

  bool mock_mode_{true};
  std::string last_led_command_;
  std::unique_ptr<medirun_sensor::SerialLineReader> reader_;
  rclcpp::TimerBase::SharedPtr timer_;
  rclcpp::Publisher<std_msgs::msg::Int32>::SharedPtr front_pub_;
  rclcpp::Publisher<std_msgs::msg::Int32>::SharedPtr front_arduino_pub_;
  rclcpp::Publisher<std_msgs::msg::Int32>::SharedPtr rear_pub_;
  rclcpp::Publisher<std_msgs::msg::Int32>::SharedPtr rear_arduino_pub_;
  rclcpp::Publisher<std_msgs::msg::Bool>::SharedPtr payload_pub_;
  rclcpp::Publisher<std_msgs::msg::Bool>::SharedPtr payload_arduino_pub_;
  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr sos_pub_;
  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr button_arduino_pub_;
  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr diag_pub_;
  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr diag_arduino_pub_;
  rclcpp::Subscription<std_msgs::msg::String>::SharedPtr led_sub_;
  rclcpp::Subscription<std_msgs::msg::String>::SharedPtr led_arduino_sub_;
};

}  // namespace

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<ArduinoBridge>());
  rclcpp::shutdown();
  return 0;
}
