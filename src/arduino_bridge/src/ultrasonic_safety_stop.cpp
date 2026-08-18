#include "arduino_bridge/ultrasonic_safety_policy.hpp"

#include <geometry_msgs/msg/twist.hpp>
#include <rclcpp/rclcpp.hpp>
#include <std_msgs/msg/int32.hpp>

#include <cstdint>
#include <memory>

namespace arduino_bridge {

// ROS 입출력과 순수 C++ 안전 정책을 연결하는 얇은 어댑터 노드다.
// 기존 토픽·파라미터·정지 동작은 유지하고 판단 로직은 UltrasonicSafetyPolicy에
// 위임한다.
class UltrasonicSafetyStopNode : public rclcpp::Node {
public:
  UltrasonicSafetyStopNode() : Node("ultrasonic_safety_stop") {
    declare_parameter("front_stop_distance_cm", 15);
    declare_parameter("rear_stop_distance_cm", 15);
    declare_parameter("hold_duration_sec", 0.0);

    cmd_pub_ = create_publisher<geometry_msgs::msg::Twist>("cmd_vel", 10);

    front_sub_ = create_subscription<std_msgs::msg::Int32>(
        "arduino/front_distance_cm", 10,
        [this](const std_msgs::msg::Int32::SharedPtr msg) {
          handleDistance(SensorSide::kFront, msg->data);
        });
    rear_sub_ = create_subscription<std_msgs::msg::Int32>(
        "arduino/rear_distance_cm", 10,
        [this](const std_msgs::msg::Int32::SharedPtr msg) {
          handleDistance(SensorSide::kRear, msg->data);
        });
    nav_cmd_sub_ = create_subscription<geometry_msgs::msg::Twist>(
        "medirun/navigation/cmd_vel_raw", 10,
        [this](const geometry_msgs::msg::Twist::SharedPtr msg) {
          handleCommand(*msg);
        });
  }

private:
  SafetyConfig currentConfig() const {
    return SafetyConfig{
        static_cast<int>(get_parameter("front_stop_distance_cm").as_int()),
        static_cast<int>(get_parameter("rear_stop_distance_cm").as_int()),
        get_parameter("hold_duration_sec").as_double(),
    };
  }

  UltrasonicSafetyPolicy::TimeNanoseconds nowNanoseconds() {
    return get_clock()->now().nanoseconds();
  }

  void handleDistance(SensorSide side, int distance_cm) {
    policy_.setConfig(currentConfig());
    policy_.updateDistance(side, distance_cm, nowNanoseconds());
    publishImmediateStopIfNeeded();
  }

  void handleCommand(geometry_msgs::msg::Twist command) {
    policy_.setConfig(currentConfig());
    const auto decision = policy_.filterCommand(
        VelocityCommand{command.linear.x, command.angular.z}, nowNanoseconds());

    if (decision.shouldStop()) {
      command.linear.x = decision.command.linear_x;
      command.angular.z = decision.command.angular_z;
      RCLCPP_WARN_THROTTLE(get_logger(), *get_clock(), 1000,
                           "%s 장애물 감지 — 센서값이 기준을 초과할 때까지 "
                           "안전 정지 (전방 %dcm, 후방 %dcm)",
                           reasonLabel(decision.reason),
                           policy_.distanceCm(SensorSide::kFront),
                           policy_.distanceCm(SensorSide::kRear));
    }

    cmd_pub_->publish(command);
  }

  void publishImmediateStopIfNeeded() {
    const auto reason = policy_.immediateStopReason();
    if (!reason.has_value()) {
      return;
    }

    cmd_pub_->publish(geometry_msgs::msg::Twist{});
    RCLCPP_WARN_THROTTLE(
        get_logger(), *get_clock(), 1000,
        "%s 초음파 감지 즉시 정지 명령 발행 (전방 %dcm, 후방 %dcm)",
        reasonLabel(*reason), policy_.distanceCm(SensorSide::kFront),
        policy_.distanceCm(SensorSide::kRear));
  }

  static const char *reasonLabel(StopReason reason) noexcept {
    switch (reason) {
    case StopReason::kRotationObstacle:
      return "회전 중";
    case StopReason::kFrontObstacle:
      return "전방";
    case StopReason::kRearObstacle:
      return "후방";
    case StopReason::kNone:
    default:
      return "";
    }
  }

  UltrasonicSafetyPolicy policy_;
  rclcpp::Publisher<geometry_msgs::msg::Twist>::SharedPtr cmd_pub_;
  rclcpp::Subscription<std_msgs::msg::Int32>::SharedPtr front_sub_;
  rclcpp::Subscription<std_msgs::msg::Int32>::SharedPtr rear_sub_;
  rclcpp::Subscription<geometry_msgs::msg::Twist>::SharedPtr nav_cmd_sub_;
};

} // namespace arduino_bridge

int main(int argc, char **argv) {
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<arduino_bridge::UltrasonicSafetyStopNode>());
  rclcpp::shutdown();
  return 0;
}
