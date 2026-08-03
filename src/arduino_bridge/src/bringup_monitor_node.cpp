#include <chrono>
#include <cmath>
#include <functional>
#include <string>

#include <geometry_msgs/msg/twist.hpp>
#include <nav_msgs/msg/occupancy_grid.hpp>
#include <nav_msgs/msg/odometry.hpp>
#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/msg/laser_scan.hpp>
#include <std_msgs/msg/string.hpp>
#include <tf2_msgs/msg/tf_message.hpp>

class BringupMonitorNode : public rclcpp::Node
{
public:
  BringupMonitorNode()
  : Node("bringup_monitor_node")
  {
    status_pub_ = create_publisher<std_msgs::msg::String>(
      "/arduino/bringup_status", rclcpp::QoS(1).transient_local());

    scan_sub_ = create_subscription<sensor_msgs::msg::LaserScan>(
      "/scan", rclcpp::SensorDataQoS(),
      [this](const sensor_msgs::msg::LaserScan::SharedPtr) {
        if (!has_scan_) {
          has_scan_ = true;
          RCLCPP_INFO(get_logger(), "[Bringup] /scan detected");
          publishStatus();
        }
      });

    odom_sub_ = create_subscription<nav_msgs::msg::Odometry>(
      "/odom", rclcpp::SystemDefaultsQoS(),
      [this](const nav_msgs::msg::Odometry::SharedPtr) {
        if (!has_odom_) {
          has_odom_ = true;
          RCLCPP_INFO(get_logger(), "[Bringup] /odom detected");
          publishStatus();
        }
      });

    tf_sub_ = create_subscription<tf2_msgs::msg::TFMessage>(
      "/tf", rclcpp::SystemDefaultsQoS(),
      [this](const tf2_msgs::msg::TFMessage::SharedPtr msg) {
        if (!msg->transforms.empty() && !has_tf_) {
          has_tf_ = true;
          RCLCPP_INFO(get_logger(), "[Bringup] /tf detected");
          publishStatus();
        }
      });

    map_sub_ = create_subscription<nav_msgs::msg::OccupancyGrid>(
      "/map", rclcpp::QoS(rclcpp::KeepLast(1)).reliable().transient_local(),
      [this](const nav_msgs::msg::OccupancyGrid::SharedPtr msg) {
        if (!has_map_ && msg->info.width > 0 && msg->info.height > 0) {
          has_map_ = true;
          RCLCPP_INFO(
            get_logger(),
            "[Bringup] /map detected (%u x %u)",
            msg->info.width,
            msg->info.height);
          publishStatus();
        }
      });

    cmd_vel_sub_ = create_subscription<geometry_msgs::msg::Twist>(
      "/cmd_vel", rclcpp::QoS(10),
      [this](const geometry_msgs::msg::Twist::SharedPtr msg) {
        const bool nonzero =
          std::fabs(msg->linear.x) > 1e-6 ||
          std::fabs(msg->linear.y) > 1e-6 ||
          std::fabs(msg->linear.z) > 1e-6 ||
          std::fabs(msg->angular.x) > 1e-6 ||
          std::fabs(msg->angular.y) > 1e-6 ||
          std::fabs(msg->angular.z) > 1e-6;
        if (nonzero && !has_cmd_vel_) {
          has_cmd_vel_ = true;
          RCLCPP_INFO(get_logger(), "[Bringup] /cmd_vel detected");
          publishStatus();
        }
      });

    timer_ = create_wall_timer(
      std::chrono::seconds(2),
      std::bind(&BringupMonitorNode::publishStatus, this));

    publishStatus();
  }

private:
  void publishStatus()
  {
    std_msgs::msg::String msg;
    msg.data =
      std::string("scan=") + (has_scan_ ? "1" : "0") +
      " odom=" + (has_odom_ ? "1" : "0") +
      " tf=" + (has_tf_ ? "1" : "0") +
      " map=" + (has_map_ ? "1" : "0") +
      " cmd_vel=" + (has_cmd_vel_ ? "1" : "0");
    status_pub_->publish(msg);
  }

  bool has_scan_ {false};
  bool has_odom_ {false};
  bool has_tf_ {false};
  bool has_map_ {false};
  bool has_cmd_vel_ {false};

  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr status_pub_;
  rclcpp::Subscription<sensor_msgs::msg::LaserScan>::SharedPtr scan_sub_;
  rclcpp::Subscription<nav_msgs::msg::Odometry>::SharedPtr odom_sub_;
  rclcpp::Subscription<tf2_msgs::msg::TFMessage>::SharedPtr tf_sub_;
  rclcpp::Subscription<nav_msgs::msg::OccupancyGrid>::SharedPtr map_sub_;
  rclcpp::Subscription<geometry_msgs::msg::Twist>::SharedPtr cmd_vel_sub_;
  rclcpp::TimerBase::SharedPtr timer_;
};

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<BringupMonitorNode>());
  rclcpp::shutdown();
  return 0;
}
