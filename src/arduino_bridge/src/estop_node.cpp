#include <rclcpp/rclcpp.hpp>
#include <std_msgs/msg/string.hpp>
#include <geometry_msgs/msg/twist.hpp>

class EstopNode : public rclcpp::Node
{
public:
  EstopNode() : Node("estop_node")
  {
    // 버튼 토픽 구독
    button_sub_ = this->create_subscription<std_msgs::msg::String>(
      "arduino/button", 10,
      std::bind(&EstopNode::buttonCallback, this, std::placeholders::_1));

    // cmd_vel 퍼블리셔
    cmd_pub_ = this->create_publisher<geometry_msgs::msg::Twist>("cmd_vel", 10);
    status_pub_ = this->create_publisher<std_msgs::msg::String>(
      "arduino/status", rclcpp::QoS(10).transient_local());

    // 정지 상태일 때 0을 계속 쏘는 타이머 (50ms = 20Hz)
    timer_ = this->create_wall_timer(
      std::chrono::milliseconds(50),
      std::bind(&EstopNode::timerCallback, this));

    RCLCPP_INFO(this->get_logger(), "E-stop 노드 시작. 버튼으로 정지/해제 토글.");
  }

private:
  void buttonCallback(const std_msgs::msg::String::SharedPtr msg)
  {
    // BTN_01을 SOS 버튼으로 사용하며, 한 번 들어올 때마다 E-stop을 토글한다.
    if (msg->data != "BTN_01") {
      return;
    }
    estop_active_ = !estop_active_;
    std_msgs::msg::String status;
    status.data = estop_active_ ? "E_STOP_ACTIVE" : "E_STOP_RELEASED";
    status_pub_->publish(status);
    RCLCPP_WARN(this->get_logger(), "E-STOP %s",
                estop_active_ ? "활성화 (로봇 정지)" : "해제 (주행 가능)");
  }

  void timerCallback()
  {
    // E-stop 활성 상태일 때만 0을 계속 publish
    if (estop_active_) {
      auto stop = geometry_msgs::msg::Twist();   // 전부 0
      cmd_pub_->publish(stop);
    }
  }

  rclcpp::Subscription<std_msgs::msg::String>::SharedPtr button_sub_;
  rclcpp::Publisher<geometry_msgs::msg::Twist>::SharedPtr cmd_pub_;
  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr status_pub_;
  rclcpp::TimerBase::SharedPtr timer_;

  bool estop_active_ = false;   // 정지 상태인가
};

int main(int argc, char **argv)
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<EstopNode>());
  rclcpp::shutdown();
  return 0;
}
