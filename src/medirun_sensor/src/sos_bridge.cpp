#include "medirun_sensor/serial_utils.hpp"

#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "rclcpp/rclcpp.hpp"
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

class SosBridge : public rclcpp::Node
{
public:
  SosBridge()
  : Node("sos_bridge")
  {
    declare_parameter("serial_port", "/dev/ttyUSB0");
    declare_parameter("baud_rate", 9600);
    declare_parameter("mock_mode", true);
    declare_parameter("poll_interval_sec", 0.1);
    declare_parameter(
      "mock_events",
      std::vector<std::string>{"BTN_01", "BTN_02", "BTN_03"});

    mock_mode_ = get_parameter("mock_mode").as_bool();
    mock_events_ = get_parameter("mock_events").as_string_array();

    sos_pub_ = create_publisher<std_msgs::msg::String>("/medirun/sensor/sos_button", 10);
    diag_pub_ = create_publisher<std_msgs::msg::String>("/medirun/sensor/sos_status", 10);

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

    RCLCPP_INFO(get_logger(), "sos_bridge ready: mock_mode=%s", mock_mode_ ? "true" : "false");
  }

private:
  void poll_serial()
  {
    std::string line;
    if (mock_mode_) {
      line = medirun_sensor::cycle_mock_lines(mock_events_, mock_index_++);
    } else if (reader_) {
      const auto value = reader_->read_line();
      if (!value.has_value()) {
        return;
      }
      line = *value;
    }

    if (line.empty()) {
      return;
    }
    if (line == "READY") {
      publish_diag("READY", "source", "sos_button_module");
      return;
    }
    std_msgs::msg::String msg;
    msg.data = line;
    sos_pub_->publish(msg);
    publish_diag("BUTTON", "button_id", line);
  }

  void publish_diag(const std::string & state, const std::string & key, const std::string & value)
  {
    std_msgs::msg::String msg;
    std::ostringstream out;
    out << "{\"state\":\"" << json_escape(state) << "\",\"" << json_escape(key) << "\":\""
        << json_escape(value) << "\"}";
    msg.data = out.str();
    diag_pub_->publish(msg);
  }

  bool mock_mode_{true};
  std::size_t mock_index_{0};
  std::vector<std::string> mock_events_;
  std::unique_ptr<medirun_sensor::SerialLineReader> reader_;
  rclcpp::TimerBase::SharedPtr timer_;
  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr sos_pub_;
  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr diag_pub_;
};

}  // namespace

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<SosBridge>());
  rclcpp::shutdown();
  return 0;
}
