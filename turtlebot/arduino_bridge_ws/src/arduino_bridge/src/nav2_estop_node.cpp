#include <rclcpp/rclcpp.hpp>
#include <rclcpp_action/rclcpp_action.hpp>
#include <std_msgs/msg/bool.hpp>
#include <nav2_msgs/action/navigate_to_pose.hpp>
#include <geometry_msgs/msg/twist.hpp>

using NavigateToPose = nav2_msgs::action::NavigateToPose;

class Nav2EstopNode : public rclcpp::Node
{
public:
  Nav2EstopNode() : Node("nav2_estop_node")
  {
    // nav2 액션 클라이언트 (navigate_to_pose 액션에 연결)
    nav_client_ = rclcpp_action::create_client<NavigateToPose>(
      this, "navigate_to_pose");

    // 버튼 토픽 구독
    button_sub_ = this->create_subscription<std_msgs::msg::Bool>(
      "arduino/button", 10,
      std::bind(&Nav2EstopNode::buttonCallback, this, std::placeholders::_1));

    // 안전용: 취소 직후 속도도 0으로 한 번 쏘기
    cmd_pub_ = this->create_publisher<geometry_msgs::msg::Twist>("cmd_vel", 10);

    RCLCPP_INFO(this->get_logger(),
      "nav2 E-stop 노드 시작. 버튼 누르면 nav2 목표를 취소합니다.");
  }

private:
  void buttonCallback(const std_msgs::msg::Bool::SharedPtr msg)
  {
    // 버튼이 눌리는 순간(false→true)에만 취소
    if (msg->data && !last_button_) {
      triggerEstop();
    }
    last_button_ = msg->data;
  }

  void triggerEstop()
  {
    RCLCPP_WARN(this->get_logger(), "E-STOP! nav2 목표 취소 요청.");

    // nav2 서버가 살아있는지 확인
    if (!nav_client_->action_server_is_ready()) {
      RCLCPP_ERROR(this->get_logger(),
        "nav2 액션 서버가 준비 안 됨. nav2가 실행 중인지 확인하세요.");
      return;
    }

    // 진행 중인 모든 goal 취소
    nav_client_->async_cancel_all_goals();

    // 안전용으로 속도 0도 한 번 publish
    auto stop = geometry_msgs::msg::Twist();
    cmd_pub_->publish(stop);

    RCLCPP_WARN(this->get_logger(), "취소 요청 전송 완료. 로봇이 정지합니다.");
  }

  rclcpp_action::Client<NavigateToPose>::SharedPtr nav_client_;
  rclcpp::Subscription<std_msgs::msg::Bool>::SharedPtr button_sub_;
  rclcpp::Publisher<geometry_msgs::msg::Twist>::SharedPtr cmd_pub_;
  bool last_button_ = false;
};

int main(int argc, char **argv)
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<Nav2EstopNode>());
  rclcpp::shutdown();
  return 0;
}
