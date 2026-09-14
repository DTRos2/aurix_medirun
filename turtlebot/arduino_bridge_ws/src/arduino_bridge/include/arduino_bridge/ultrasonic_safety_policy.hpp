#ifndef ARDUINO_BRIDGE__ULTRASONIC_SAFETY_POLICY_HPP_
#define ARDUINO_BRIDGE__ULTRASONIC_SAFETY_POLICY_HPP_

#include <cstdint>
#include <optional>

namespace arduino_bridge {

enum class SensorSide {
  kFront,
  kRear,
};

enum class StopReason {
  kNone,
  kFrontObstacle,
  kRearObstacle,
  kRotationObstacle,
};

struct SafetyConfig {
  int front_stop_distance_cm{15};
  int rear_stop_distance_cm{15};
  double hold_duration_sec{0.0};
};

struct VelocityCommand {
  double linear_x{0.0};
  double angular_z{0.0};
};

struct SafetyDecision {
  VelocityCommand command;
  StopReason reason{StopReason::kNone};

  bool shouldStop() const noexcept;
};

// ROS에 의존하지 않는 초음파 안전 판단 정책이다.
// 시간은 호출자가 사용하는 clock의 나노초 값으로 전달한다. 따라서 ROS time,
// steady clock 또는 시험용 가상 clock 중 어느 것이든 같은 정책을 재사용할 수
// 있다. 콜백을 여러 스레드에서 호출한다면 외부에서 동기화해야 한다.
class UltrasonicSafetyPolicy {
public:
  using TimeNanoseconds = std::int64_t;

  explicit UltrasonicSafetyPolicy(const SafetyConfig &config = SafetyConfig{});

  void setConfig(const SafetyConfig &config) noexcept;
  const SafetyConfig &config() const noexcept;

  void updateDistance(SensorSide side, int distance_cm, TimeNanoseconds now_ns);
  SafetyDecision filterCommand(const VelocityCommand &command,
                               TimeNanoseconds now_ns);

  // 마지막 원본 속도 명령이 현재 센서 상태에서 위험하면 정지 이유를 반환한다.
  // 센서 콜백만 새로 들어오고 Nav2 속도 콜백이 잠시 끊겨도 즉시 0을 발행할 때
  // 사용한다.
  std::optional<StopReason> immediateStopReason() const noexcept;

  int distanceCm(SensorSide side) const noexcept;
  bool isHolding(SensorSide side) const noexcept;

private:
  struct ProximityState {
    int distance_cm{400};
    bool holding{false};
    TimeNanoseconds hold_started_ns{0};
  };

  static constexpr double kMotionEpsilon = 1.0e-6;

  ProximityState &stateFor(SensorSide side) noexcept;
  const ProximityState &stateFor(SensorSide side) const noexcept;
  int stopDistanceCm(SensorSide side) const noexcept;
  void refreshState(SensorSide side, TimeNanoseconds now_ns);
  StopReason evaluateStopReason(const VelocityCommand &command) const noexcept;

  SafetyConfig config_;
  ProximityState front_;
  ProximityState rear_;
  bool has_last_command_{false};
  VelocityCommand last_command_;
};

} // namespace arduino_bridge

#endif // ARDUINO_BRIDGE__ULTRASONIC_SAFETY_POLICY_HPP_
