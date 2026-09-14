#include "arduino_bridge/ultrasonic_safety_policy.hpp"

#include <geometry_msgs/msg/twist.hpp>
#include <rclcpp/rclcpp.hpp>
#include <std_msgs/msg/bool.hpp>
#include <std_msgs/msg/int32.hpp>
#include <std_msgs/msg/string.hpp>

#include <cstdint>
#include <memory>
#include <string>

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
    // 정지 시작/해제 순간에만 방송(레벨이 아니라 엣지) — arrival_return_manager가
    // 이걸로 복귀 중 "비켜주세요" 음성을 딱 1회 트리거한다.
    obstacle_stop_pub_ = create_publisher<std_msgs::msg::Bool>("medirun/safety/obstacle_stop", 10);

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
          // ArUco 전면주차가 진행 중일 때는 Nav2 쪽 잔여 발행(정지 직후 늦게
          // 도착하는 메시지 등)이 파킹 저속 명령을 덮어쓰지 않도록 무시한다.
          if (parking_active_) {
            return;
          }
          handleCommand(*msg);
        });
    parking_cmd_sub_ = create_subscription<geometry_msgs::msg::Twist>(
        "medirun/parking/cmd_vel_raw", 10,
        [this](const geometry_msgs::msg::Twist::SharedPtr msg) {
          handleCommand(*msg);
        });
    parking_state_sub_ = create_subscription<std_msgs::msg::String>(
        "medirun/parking/state", 10,
        [this](const std_msgs::msg::String::SharedPtr msg) {
          parking_active_ = msg->data != "IDLE" && msg->data != "PARKED" && msg->data != "FAILED";
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
    updateStopState(decision.shouldStop());

    cmd_pub_->publish(command);
  }

  void publishImmediateStopIfNeeded() {
    const auto reason = policy_.immediateStopReason();
    if (!reason.has_value()) {
      return;
    }

    cmd_pub_->publish(geometry_msgs::msg::Twist{});
    updateStopState(true);
    RCLCPP_WARN_THROTTLE(
        get_logger(), *get_clock(), 1000,
        "%s 초음파 감지 즉시 정지 명령 발행 (전방 %dcm, 후방 %dcm)",
        reasonLabel(*reason), policy_.distanceCm(SensorSide::kFront),
        policy_.distanceCm(SensorSide::kRear));
  }

  void updateStopState(bool active) {
    if (active == stop_active_) {
      return;
    }
    stop_active_ = active;
    std_msgs::msg::Bool msg;
    msg.data = active;
    obstacle_stop_pub_->publish(msg);
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
  bool stop_active_{false};
  rclcpp::Publisher<geometry_msgs::msg::Twist>::SharedPtr cmd_pub_;
  rclcpp::Publisher<std_msgs::msg::Bool>::SharedPtr obstacle_stop_pub_;
  rclcpp::Subscription<std_msgs::msg::Int32>::SharedPtr front_sub_;
  rclcpp::Subscription<std_msgs::msg::Int32>::SharedPtr rear_sub_;
  rclcpp::Subscription<geometry_msgs::msg::Twist>::SharedPtr nav_cmd_sub_;
  rclcpp::Subscription<geometry_msgs::msg::Twist>::SharedPtr parking_cmd_sub_;
  rclcpp::Subscription<std_msgs::msg::String>::SharedPtr parking_state_sub_;
  bool parking_active_{false};
};

} // namespace arduino_bridge

int main(int argc, char **argv) {
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<arduino_bridge::UltrasonicSafetyStopNode>());
  rclcpp::shutdown();
  return 0;
}
