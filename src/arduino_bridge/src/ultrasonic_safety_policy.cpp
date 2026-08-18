#include "arduino_bridge/ultrasonic_safety_policy.hpp"

#include <cmath>

namespace arduino_bridge {

bool SafetyDecision::shouldStop() const noexcept {
  return reason != StopReason::kNone;
}

UltrasonicSafetyPolicy::UltrasonicSafetyPolicy(const SafetyConfig &config)
    : config_(config) {}

void UltrasonicSafetyPolicy::setConfig(const SafetyConfig &config) noexcept {
  config_ = config;
}

const SafetyConfig &UltrasonicSafetyPolicy::config() const noexcept {
  return config_;
}

void UltrasonicSafetyPolicy::updateDistance(SensorSide side, int distance_cm,
                                            TimeNanoseconds now_ns) {
  stateFor(side).distance_cm = distance_cm;
  refreshState(side, now_ns);
}

SafetyDecision
UltrasonicSafetyPolicy::filterCommand(const VelocityCommand &command,
                                      TimeNanoseconds now_ns) {
  last_command_ = command;
  has_last_command_ = true;

  // 속도 메시지 시점에도 양쪽 상태를 다시 평가한다. 센서값은 그대로여도
  // hold_duration_sec가 끝났거나 런타임 파라미터가 바뀐 경우를 반영하기
  // 위해서다.
  refreshState(SensorSide::kFront, now_ns);
  refreshState(SensorSide::kRear, now_ns);

  SafetyDecision decision{command, evaluateStopReason(command)};
  if (decision.shouldStop()) {
    decision.command.linear_x = 0.0;
    decision.command.angular_z = 0.0;
  }
  return decision;
}

std::optional<StopReason>
UltrasonicSafetyPolicy::immediateStopReason() const noexcept {
  if (!has_last_command_) {
    return std::nullopt;
  }

  const StopReason reason = evaluateStopReason(last_command_);
  if (reason == StopReason::kNone) {
    return std::nullopt;
  }
  return reason;
}

int UltrasonicSafetyPolicy::distanceCm(SensorSide side) const noexcept {
  return stateFor(side).distance_cm;
}

bool UltrasonicSafetyPolicy::isHolding(SensorSide side) const noexcept {
  return stateFor(side).holding;
}

UltrasonicSafetyPolicy::ProximityState &
UltrasonicSafetyPolicy::stateFor(SensorSide side) noexcept {
  return side == SensorSide::kFront ? front_ : rear_;
}

const UltrasonicSafetyPolicy::ProximityState &
UltrasonicSafetyPolicy::stateFor(SensorSide side) const noexcept {
  return side == SensorSide::kFront ? front_ : rear_;
}

int UltrasonicSafetyPolicy::stopDistanceCm(SensorSide side) const noexcept {
  return side == SensorSide::kFront ? config_.front_stop_distance_cm
                                    : config_.rear_stop_distance_cm;
}

void UltrasonicSafetyPolicy::refreshState(SensorSide side,
                                          TimeNanoseconds now_ns) {
  auto &state = stateFor(side);
  if (state.distance_cm <= stopDistanceCm(side)) {
    state.holding = true;
    state.hold_started_ns = now_ns;
    return;
  }

  const double elapsed_sec =
      static_cast<double>(now_ns - state.hold_started_ns) / 1'000'000'000.0;
  if (state.holding && elapsed_sec < config_.hold_duration_sec) {
    return;
  }
  state.holding = false;
}

StopReason UltrasonicSafetyPolicy::evaluateStopReason(
    const VelocityCommand &command) const noexcept {
  const bool front_blocking = front_.holding && command.linear_x > 0.0;
  const bool rear_blocking = rear_.holding && command.linear_x < 0.0;
  const bool rotation_blocking = (front_.holding || rear_.holding) &&
                                 std::abs(command.angular_z) > kMotionEpsilon;

  // 기존 로그와 동일하게 회전이 섞인 명령은 회전 위험을 가장 먼저 보고한다.
  if (rotation_blocking) {
    return StopReason::kRotationObstacle;
  }
  if (front_blocking) {
    return StopReason::kFrontObstacle;
  }
  if (rear_blocking) {
    return StopReason::kRearObstacle;
  }
  return StopReason::kNone;
}

} // namespace arduino_bridge
