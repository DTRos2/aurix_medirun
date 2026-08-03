#include "medirun_sensor/serial_utils.hpp"

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>

namespace medirun_sensor
{
namespace
{

speed_t to_baud(int baud_rate)
{
  switch (baud_rate) {
    case 9600:
      return B9600;
    case 19200:
      return B19200;
    case 38400:
      return B38400;
    case 57600:
      return B57600;
    case 115200:
      return B115200;
    default:
      return B9600;
  }
}

std::string trim(const std::string & value)
{
  const auto begin = value.find_first_not_of(" \r\n\t");
  if (begin == std::string::npos) {
    return "";
  }
  const auto end = value.find_last_not_of(" \r\n\t");
  return value.substr(begin, end - begin + 1);
}

}  // namespace

SerialLineReader::SerialLineReader(SerialConfig config)
: config_(std::move(config))
{
}

SerialLineReader::~SerialLineReader()
{
  close();
}

bool SerialLineReader::open()
{
  close();
  fd_ = ::open(config_.port.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
  if (fd_ < 0) {
    return false;
  }

  termios tty {};
  if (tcgetattr(fd_, &tty) != 0) {
    close();
    return false;
  }

  cfsetispeed(&tty, to_baud(config_.baud_rate));
  cfsetospeed(&tty, to_baud(config_.baud_rate));
  tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;
  tty.c_iflag &= ~(IGNBRK | IXON | IXOFF | IXANY);
  tty.c_lflag = 0;
  tty.c_oflag = 0;
  tty.c_cc[VMIN] = 0;
  tty.c_cc[VTIME] = static_cast<cc_t>(std::max(config_.timeout_ms / 100, 1));
  tty.c_cflag |= (CLOCAL | CREAD);
  tty.c_cflag &= ~(PARENB | PARODD);
  tty.c_cflag &= ~CSTOPB;
  tty.c_cflag &= ~CRTSCTS;

  if (tcsetattr(fd_, TCSANOW, &tty) != 0) {
    close();
    return false;
  }
  return true;
}

std::optional<std::string> SerialLineReader::read_line()
{
  if (fd_ < 0) {
    return std::nullopt;
  }

  char chunk[256];
  const ssize_t bytes_read = ::read(fd_, chunk, sizeof(chunk));
  if (bytes_read < 0) {
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      return std::nullopt;
    }
    return std::nullopt;
  }
  if (bytes_read == 0) {
    return std::nullopt;
  }

  buffer_.append(chunk, static_cast<std::size_t>(bytes_read));
  const auto newline_pos = buffer_.find_first_of("\r\n");
  if (newline_pos == std::string::npos) {
    return std::nullopt;
  }

  std::string line = trim(buffer_.substr(0, newline_pos));
  const auto next_data = buffer_.find_first_not_of("\r\n", newline_pos);
  buffer_ = next_data == std::string::npos ? "" : buffer_.substr(next_data);
  if (line.empty()) {
    return std::nullopt;
  }
  return line;
}

void SerialLineReader::write_line(const std::string & line)
{
  if (fd_ < 0) {
    return;
  }
  const std::string payload = line + "\n";
  (void)::write(fd_, payload.c_str(), payload.size());
}

void SerialLineReader::close()
{
  if (fd_ >= 0) {
    ::close(fd_);
    fd_ = -1;
  }
}

std::pair<std::string, std::string> parse_key_value_line(const std::string & line)
{
  const auto pos = line.find(':');
  if (pos == std::string::npos) {
    return {trim(line), ""};
  }
  return {trim(line.substr(0, pos)), trim(line.substr(pos + 1))};
}

std::string cycle_mock_lines(const std::vector<std::string> & lines, std::size_t index)
{
  if (lines.empty()) {
    return "";
  }
  return lines[index % lines.size()];
}

}  // namespace medirun_sensor
