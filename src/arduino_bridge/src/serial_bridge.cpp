#include <rclcpp/rclcpp.hpp>
#include <std_msgs/msg/bool.hpp>
#include <std_msgs/msg/int32.hpp>
#include <std_msgs/msg/string.hpp>

#include <fcntl.h>
#include <termios.h>
#include <unistd.h>

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>

class SerialBridge : public rclcpp::Node
{
public:
  SerialBridge() : Node("serial_bridge")
  {
    port_ = declare_parameter<std::string>("port", "/dev/ttyACM0");
    const int baud = declare_parameter<int>("baud", 9600);

    if (!openSerial(port_, baud)) {
      RCLCPP_ERROR(get_logger(), "시리얼 포트 열기 실패: %s", port_.c_str());
      throw std::runtime_error("serial open failed");
    }
    RCLCPP_INFO(get_logger(), "시리얼 포트 연결됨: %s (%d baud)", port_.c_str(), baud);

    button_pub_ = create_publisher<std_msgs::msg::String>("/arduino/button", 10);
    front_distance_pub_ =
      create_publisher<std_msgs::msg::Int32>("/arduino/front_distance_cm", 10);
    rear_distance_pub_ =
      create_publisher<std_msgs::msg::Int32>("/arduino/rear_distance_cm", 10);
    payload_pub_ = create_publisher<std_msgs::msg::Bool>("/arduino/payload_loaded", 10);
    status_pub_ = create_publisher<std_msgs::msg::String>(
      "/arduino/status", rclcpp::QoS(10).transient_local());

    buzzer_sub_ = create_subscription<std_msgs::msg::Bool>(
      "/arduino/buzzer", 10,
      [this](const std_msgs::msg::Bool::SharedPtr msg) {
        sendCommand(msg->data ? "BUZZER:1" : "BUZZER:0");
      });

    // 통합 펌웨어가 상태별 점등/점멸을 직접 처리하므로 상태 이름을 그대로 전달한다.
    // 예: IDLE -> LED:IDLE, RUN -> LED:RUN
    led_sub_ = create_subscription<std_msgs::msg::String>(
      "/arduino/led", 10,
      [this](const std_msgs::msg::String::SharedPtr msg) {
        if (msg->data.empty()) {
          RCLCPP_WARN(get_logger(), "빈 LED 명령은 전송하지 않습니다.");
          return;
        }
        std::string state =
          msg->data.rfind("LED:", 0) == 0 ? msg->data.substr(4) : msg->data;

        if (
          state != "IDLE" && state != "RUN" && state != "ARRIVE" &&
          state != "STOP" && state != "ERROR" && state != "DOCKED")
        {
          RCLCPP_WARN(get_logger(), "지원하지 않는 LED 상태: %s", state.c_str());
          return;
        }

        sendCommand("LED:" + state);
      });

    timer_ = create_wall_timer(
      std::chrono::milliseconds(20), std::bind(&SerialBridge::readSerial, this));
  }

  ~SerialBridge() override
  {
    if (fd_ >= 0) {
      close(fd_);
    }
  }

private:
  bool openSerial(const std::string & port, int baud)
  {
    fd_ = open(port.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd_ < 0) {
      return false;
    }

    termios tty{};
    if (tcgetattr(fd_, &tty) != 0) {
      close(fd_);
      fd_ = -1;
      return false;
    }

    const speed_t speed = (baud == 115200) ? B115200 : B9600;
    if (baud != 9600 && baud != 115200) {
      RCLCPP_WARN(get_logger(), "지원하지 않는 baud %d: 9600을 사용합니다.", baud);
    }
    cfsetispeed(&tty, speed);
    cfsetospeed(&tty, speed);

    tty.c_cflag |= CLOCAL | CREAD;
    tty.c_cflag &= ~CSIZE;
    tty.c_cflag |= CS8;
    tty.c_cflag &= ~PARENB;
    tty.c_cflag &= ~CSTOPB;
    tty.c_lflag = 0;
    tty.c_oflag = 0;
    tty.c_iflag = 0;

    if (tcsetattr(fd_, TCSANOW, &tty) != 0) {
      close(fd_);
      fd_ = -1;
      return false;
    }
    return true;
  }

  void sendCommand(const std::string & command)
  {
    const std::string wire_data = command + "\n";
    const ssize_t written = write(fd_, wire_data.data(), wire_data.size());
    if (written != static_cast<ssize_t>(wire_data.size())) {
      RCLCPP_ERROR(
        get_logger(), "시리얼 전송 실패 (%s): %s", command.c_str(), std::strerror(errno));
      return;
    }
    RCLCPP_INFO(get_logger(), "시리얼 명령 전송: %s", command.c_str());
  }

  void readSerial()
  {
    char buffer[256];
    const ssize_t count = read(fd_, buffer, sizeof(buffer));
    if (count < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
      RCLCPP_ERROR_THROTTLE(
        get_logger(), *get_clock(), 2000, "시리얼 읽기 실패: %s", std::strerror(errno));
      return;
    }

    for (ssize_t i = 0; i < count; ++i) {
      const char c = buffer[i];
      if (c == '\n') {
        if (!rx_buffer_.empty()) {
          processLine(rx_buffer_);
          rx_buffer_.clear();
        }
      } else if (c != '\r') {
        rx_buffer_ += c;
        if (rx_buffer_.size() > 512) {
          RCLCPP_WARN(get_logger(), "너무 긴 시리얼 입력을 폐기합니다.");
          rx_buffer_.clear();
        }
      }
    }
  }

  bool parseInt(const std::string & text, int32_t & value) const
  {
    char * end = nullptr;
    errno = 0;
    const long parsed = std::strtol(text.c_str(), &end, 10);
    if (errno != 0 || end == text.c_str() || *end != '\0' ||
      parsed < std::numeric_limits<int32_t>::min() ||
      parsed > std::numeric_limits<int32_t>::max())
    {
      return false;
    }
    value = static_cast<int32_t>(parsed);
    return true;
  }

  void publishStatus(const std::string & value)
  {
    std_msgs::msg::String msg;
    msg.data = value;
    status_pub_->publish(msg);
  }

  void processLine(const std::string & line)
  {
    if (line == "READY") {
      publishStatus("READY");
      RCLCPP_INFO(get_logger(), "아두이노 준비됨 (READY)");
      return;
    }

    if (line == "BTN_01" || line == "BTN_02" || line == "BTN_03") {
      std_msgs::msg::String msg;
      msg.data = line;
      button_pub_->publish(msg);
      RCLCPP_INFO(get_logger(), "버튼 입력: %s", line.c_str());
      return;
    }
    // 단일 버튼 펌웨어의 BTN:1(눌림)을 E-stop 버튼 BTN_01로 변환한다.
    // BTN:0(뗌)은 토글이 두 번 발생하지 않도록 발행하지 않는다.
    if (line == "BTN:1") {
      std_msgs::msg::String msg;
      msg.data = "BTN_01";
      button_pub_->publish(msg);
      RCLCPP_INFO(get_logger(), "버튼 입력: BTN_01");
      return;
    }
    if (line == "BTN:0") {
      return;
    }

    if (line.rfind("DIST_REAR:", 0) == 0) {
      publishInteger(line, "DIST_REAR:", rear_distance_pub_);
      return;
    }
    if (line.rfind("DIST:", 0) == 0) {
      publishInteger(line, "DIST:", front_distance_pub_);
      return;
    }

    if (line == "HALL:0" || line == "HALL:1") {
      std_msgs::msg::Bool msg;
      msg.data = (line == "HALL:1");
      payload_pub_->publish(msg);
      RCLCPP_INFO(get_logger(), "적재 감지: %s", msg.data ? "적재됨" : "비어 있음");
      return;
    }

    if (line.rfind("LED:", 0) == 0) {
      publishStatus("LED_ACK:" + line.substr(4));
      RCLCPP_INFO(get_logger(), "LED 상태 응답: %s", line.substr(4).c_str());
      return;
    }

    RCLCPP_WARN(get_logger(), "알 수 없는 시리얼 입력: %s", line.c_str());
  }

  void publishInteger(
    const std::string & line, const std::string & prefix,
    const rclcpp::Publisher<std_msgs::msg::Int32>::SharedPtr & publisher)
  {
    int32_t value = 0;
    if (!parseInt(line.substr(prefix.size()), value)) {
      RCLCPP_WARN(get_logger(), "잘못된 센서 값: %s", line.c_str());
      return;
    }
    std_msgs::msg::Int32 msg;
    msg.data = value;
    publisher->publish(msg);
    RCLCPP_INFO(get_logger(), "%s %d cm", prefix.c_str(), value);
  }

  std::string port_;
  int fd_ = -1;
  std::string rx_buffer_;
  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr button_pub_;
  rclcpp::Publisher<std_msgs::msg::Int32>::SharedPtr front_distance_pub_;
  rclcpp::Publisher<std_msgs::msg::Int32>::SharedPtr rear_distance_pub_;
  rclcpp::Publisher<std_msgs::msg::Bool>::SharedPtr payload_pub_;
  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr status_pub_;
  rclcpp::Subscription<std_msgs::msg::Bool>::SharedPtr buzzer_sub_;
  rclcpp::Subscription<std_msgs::msg::String>::SharedPtr led_sub_;
  rclcpp::TimerBase::SharedPtr timer_;
};

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<SerialBridge>());
  rclcpp::shutdown();
  return 0;
}
