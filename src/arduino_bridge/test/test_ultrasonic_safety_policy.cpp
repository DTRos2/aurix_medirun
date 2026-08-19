#include "arduino_bridge/ultrasonic_safety_policy.hpp"

#include <gtest/gtest.h>

namespace arduino_bridge {
namespace {

constexpr UltrasonicSafetyPolicy::TimeNanoseconds kOneSecond = 1'000'000'000LL;

TEST(UltrasonicSafetyPolicyTest, PreservesStraightDirectionRulesAtThreshold) {
  UltrasonicSafetyPolicy policy;

  policy.updateDistance(SensorSide::kFront, 15, 0);
  auto decision = policy.filterCommand(VelocityCommand{0.2, 0.0}, 0);
  EXPECT_TRUE(decision.shouldStop());
  EXPECT_EQ(decision.reason, StopReason::kFrontObstacle);
  EXPECT_DOUBLE_EQ(decision.command.linear_x, 0.0);

  decision = policy.filterCommand(VelocityCommand{-0.2, 0.0}, 0);
  EXPECT_FALSE(decision.shouldStop());
  EXPECT_DOUBLE_EQ(decision.command.linear_x, -0.2);

  policy.updateDistance(SensorSide::kFront, 100, 0);
  policy.updateDistance(SensorSide::kRear, 15, 0);
  decision = policy.filterCommand(VelocityCommand{-0.2, 0.0}, 0);
  EXPECT_TRUE(decision.shouldStop());
  EXPECT_EQ(decision.reason, StopReason::kRearObstacle);

  decision = policy.filterCommand(VelocityCommand{0.2, 0.0}, 0);
  EXPECT_FALSE(decision.shouldStop());
  EXPECT_DOUBLE_EQ(decision.command.linear_x, 0.2);
}

TEST(UltrasonicSafetyPolicyTest, BlocksPureInPlaceRotationFromEitherSensor) {
  UltrasonicSafetyPolicy policy;

  // 순수 제자리 회전(선속도 0)은 전후방 상관없이 걸리면 정지.
  policy.updateDistance(SensorSide::kRear, 10, 0);
  auto decision = policy.filterCommand(VelocityCommand{0.0, 0.5}, 0);
  EXPECT_TRUE(decision.shouldStop());
  EXPECT_EQ(decision.reason, StopReason::kRotationObstacle);
  EXPECT_DOUBLE_EQ(decision.command.angular_z, 0.0);

  policy.updateDistance(SensorSide::kRear, 100, 0);
  policy.updateDistance(SensorSide::kFront, 10, 0);
  decision = policy.filterCommand(VelocityCommand{0.0, -0.5}, 0);
  EXPECT_TRUE(decision.shouldStop());
  EXPECT_EQ(decision.reason, StopReason::kRotationObstacle);
  EXPECT_DOUBLE_EQ(decision.command.angular_z, 0.0);
}

TEST(UltrasonicSafetyPolicyTest, CurvedDrivingFollowsDirectionOnlyRules) {
  UltrasonicSafetyPolicy policy;

  // 조향이 섞인 후진(linear_x<0)은 후방 센서만 본다 — 전방이 걸려도 안 멈춘다.
  policy.updateDistance(SensorSide::kFront, 10, 0);
  auto decision = policy.filterCommand(VelocityCommand{-0.1, -0.5}, 0);
  EXPECT_FALSE(decision.shouldStop());
  EXPECT_DOUBLE_EQ(decision.command.linear_x, -0.1);
  EXPECT_DOUBLE_EQ(decision.command.angular_z, -0.5);

  // 조향이 섞인 전진(linear_x>0)은 전방이 걸리면 멈춘다.
  decision = policy.filterCommand(VelocityCommand{0.1, 0.3}, 0);
  EXPECT_TRUE(decision.shouldStop());
  EXPECT_EQ(decision.reason, StopReason::kFrontObstacle);

  // 조향이 섞인 전진은 후방이 걸려도 안 멈춘다(이게 이번 수정의 핵심).
  policy.updateDistance(SensorSide::kFront, 100, 0);
  policy.updateDistance(SensorSide::kRear, 10, 0);
  decision = policy.filterCommand(VelocityCommand{0.1, 0.3}, 0);
  EXPECT_FALSE(decision.shouldStop());
  EXPECT_DOUBLE_EQ(decision.command.linear_x, 0.1);
  EXPECT_DOUBLE_EQ(decision.command.angular_z, 0.3);
}

TEST(UltrasonicSafetyPolicyTest,
     RequestsImmediateStopWhenSensorArrivesAfterCommand) {
  UltrasonicSafetyPolicy policy;

  auto decision = policy.filterCommand(VelocityCommand{-0.2, 0.0}, 0);
  ASSERT_FALSE(decision.shouldStop());
  EXPECT_FALSE(policy.immediateStopReason().has_value());

  policy.updateDistance(SensorSide::kRear, 8, 1);
  const auto reason = policy.immediateStopReason();
  ASSERT_TRUE(reason.has_value());
  EXPECT_EQ(*reason, StopReason::kRearObstacle);
}

TEST(UltrasonicSafetyPolicyTest,
     ReleasesImmediatelyAboveThresholdWithDefaultConfig) {
  UltrasonicSafetyPolicy policy;

  policy.updateDistance(SensorSide::kFront, 8, 0);
  EXPECT_TRUE(policy.filterCommand(VelocityCommand{0.2, 0.0}, 0).shouldStop());

  policy.updateDistance(SensorSide::kFront, 16, 1);
  const auto decision = policy.filterCommand(VelocityCommand{0.2, 0.0}, 1);
  EXPECT_FALSE(decision.shouldStop());
  EXPECT_DOUBLE_EQ(decision.command.linear_x, 0.2);
}

TEST(UltrasonicSafetyPolicyTest, SupportsOptionalPositiveHoldDuration) {
  UltrasonicSafetyPolicy policy(SafetyConfig{15, 15, 2.0});

  policy.updateDistance(SensorSide::kFront, 8, 0);
  policy.updateDistance(SensorSide::kFront, 100, kOneSecond);
  EXPECT_TRUE(
      policy.filterCommand(VelocityCommand{0.2, 0.0}, kOneSecond).shouldStop());

  const auto decision =
      policy.filterCommand(VelocityCommand{0.2, 0.0}, 2 * kOneSecond);
  EXPECT_FALSE(decision.shouldStop());
}

TEST(UltrasonicSafetyPolicyTest, AppliesRuntimeConfigurationChanges) {
  UltrasonicSafetyPolicy policy;
  policy.updateDistance(SensorSide::kRear, 20, 0);
  EXPECT_FALSE(
      policy.filterCommand(VelocityCommand{-0.2, 0.0}, 0).shouldStop());

  policy.setConfig(SafetyConfig{15, 25, 0.0});
  const auto decision = policy.filterCommand(VelocityCommand{-0.2, 0.0}, 1);
  EXPECT_TRUE(decision.shouldStop());
  EXPECT_EQ(decision.reason, StopReason::kRearObstacle);
}

} // namespace
} // namespace arduino_bridge
