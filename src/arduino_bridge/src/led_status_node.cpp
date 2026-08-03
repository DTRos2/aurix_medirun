// 로봇 상태를 Arduino LED 프로토콜의 상태 이름으로 변환한다.
#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/string.hpp"

using namespace std::chrono_literals;

class LedStatusNode : public rclcpp::Node {
public:
  LedStatusNode() : Node("led_status_node") {
    button_sub_ = create_subscription<std_msgs::msg::String>(
        "/arduino/button", 10,
        [this](std_msgs::msg::String::SharedPtr msg) {
          if (msg->data == "BTN_01") {
            estop_ = !estop_;
          }
        });

    // v2용 — /robot_status 안 들어오면 driving_ 은 -1(미수신) 유지
    status_sub_ = create_subscription<std_msgs::msg::String>(
        "/robot_status", 10,
        [this](std_msgs::msg::String::SharedPtr msg) {
          driving_ = (msg->data == "driving") ? 1 : 0;
        });

    const auto latched_qos = rclcpp::QoS(10).transient_local();
    led_pub_ = create_publisher<std_msgs::msg::String>("/arduino/led", latched_qos);
    status_pub_ = create_publisher<std_msgs::msg::String>("/arduino/status", latched_qos);

    publishStatus("주행준비되었습니다");
    publishLed("IDLE");  // bringup 완료 시 대기(초록) 상태
    RCLCPP_INFO(get_logger(), "주행준비되었습니다");

    timer_ = create_wall_timer(200ms, [this]() { tick(); });  // 5Hz
  }

private:
  std::string decide() const {
    if (estop_)        return "STOP";
    if (driving_ < 0)  return "IDLE";
    return driving_ ? "RUN" : "IDLE";
  }

  void tick() {
    const std::string state = decide();
    ++ticks_since_publish_;
    // 시리얼 브리지가 나중에 시작되거나 재연결되는 경우에도 상태를 다시 전달한다.
    if (state != last_ || ticks_since_publish_ >= 25) {
      publishLed(state);
    }
  }

  void publishLed(const std::string & state) {
    std_msgs::msg::String msg;
    msg.data = state;
    led_pub_->publish(msg);
    last_ = state;
    ticks_since_publish_ = 0;
  }

  void publishStatus(const std::string & status) {
    std_msgs::msg::String msg;
    msg.data = status;
    status_pub_->publish(msg);
  }

  rclcpp::Subscription<std_msgs::msg::String>::SharedPtr button_sub_;
  rclcpp::Subscription<std_msgs::msg::String>::SharedPtr status_sub_;
  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr    led_pub_;
  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr    status_pub_;
  rclcpp::TimerBase::SharedPtr timer_;

  bool estop_   = false;
  int  driving_ = -1;    // -1=미수신(v1), 0=대기, 1=주행
  std::string last_;
  int ticks_since_publish_ = 0;
};

int main(int argc, char** argv) {
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<LedStatusNode>());
  rclcpp::shutdown();
  return 0;
}
