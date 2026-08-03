#include <algorithm>
#include <cmath>
#include <memory>
#include <string>

#include <geometry_msgs/msg/twist.hpp>
#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/msg/joy.hpp>

class Ps4TeleopNode : public rclcpp::Node
{
public:
  Ps4TeleopNode()
  : Node("ps4_teleop_node")
  {
    linear_axis_ = declare_parameter("linear_axis", 1);
    angular_axis_ = declare_parameter("angular_axis", 3);
    enable_button_ = declare_parameter("enable_button", 4);
    turbo_button_ = declare_parameter("turbo_button", 5);
    linear_scale_ = declare_parameter("linear_scale", 0.22);
    angular_scale_ = declare_parameter("angular_scale", 2.84);
    turbo_linear_scale_ = declare_parameter("turbo_linear_scale", 0.26);
    turbo_angular_scale_ = declare_parameter("turbo_angular_scale", 3.2);

    cmd_pub_ = create_publisher<geometry_msgs::msg::Twist>("cmd_vel", 10);
    joy_sub_ = create_subscription<sensor_msgs::msg::Joy>(
      "joy", 10,
      std::bind(&Ps4TeleopNode::joyCallback, this, std::placeholders::_1));

    RCLCPP_INFO(
      get_logger(),
      "PS4 teleop started: linear_axis=%d angular_axis=%d enable_button=%d turbo_button=%d",
      linear_axis_, angular_axis_, enable_button_, turbo_button_);
  }

private:
  void joyCallback(const sensor_msgs::msg::Joy::SharedPtr msg)
  {
    const bool enabled = buttonPressed(msg->buttons, enable_button_);
    const bool turbo = buttonPressed(msg->buttons, turbo_button_);

    geometry_msgs::msg::Twist twist;
    if (enabled || turbo) {
      const double linear_input = axisValue(msg->axes, linear_axis_);
      const double angular_input = axisValue(msg->axes, angular_axis_);
      const double linear_scale = turbo ? turbo_linear_scale_ : linear_scale_;
      const double angular_scale = turbo ? turbo_angular_scale_ : angular_scale_;

      twist.linear.x = linear_input * linear_scale;
      twist.angular.z = angular_input * angular_scale;
    }

    if (commandChanged(twist)) {
      cmd_pub_->publish(twist);
      last_cmd_ = twist;
    }
  }

  static bool buttonPressed(const std::vector<int32_t> & buttons, int index)
  {
    return index >= 0 &&
           static_cast<size_t>(index) < buttons.size() &&
           buttons[static_cast<size_t>(index)] != 0;
  }

  static double axisValue(const std::vector<float> & axes, int index)
  {
    if (index < 0 || static_cast<size_t>(index) >= axes.size()) {
      return 0.0;
    }
    return std::clamp(static_cast<double>(axes[static_cast<size_t>(index)]), -1.0, 1.0);
  }

  bool commandChanged(const geometry_msgs::msg::Twist & twist) const
  {
    constexpr double epsilon = 1e-6;
    return std::fabs(twist.linear.x - last_cmd_.linear.x) > epsilon ||
           std::fabs(twist.angular.z - last_cmd_.angular.z) > epsilon;
  }

  int linear_axis_;
  int angular_axis_;
  int enable_button_;
  int turbo_button_;
  double linear_scale_;
  double angular_scale_;
  double turbo_linear_scale_;
  double turbo_angular_scale_;

  geometry_msgs::msg::Twist last_cmd_{};
  rclcpp::Publisher<geometry_msgs::msg::Twist>::SharedPtr cmd_pub_;
  rclcpp::Subscription<sensor_msgs::msg::Joy>::SharedPtr joy_sub_;
};

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<Ps4TeleopNode>());
  rclcpp::shutdown();
  return 0;
}
