#pragma once

#include <optional>
#include <string>
#include <vector>

namespace medirun_sensor
{

struct SerialConfig
{
  std::string port;
  int baud_rate{9600};
  int timeout_ms{100};
};

class SerialLineReader
{
public:
  explicit SerialLineReader(SerialConfig config);
  ~SerialLineReader();

  bool open();
  std::optional<std::string> read_line();
  void write_line(const std::string & line);
  void close();

private:
  SerialConfig config_;
  int fd_{-1};
  std::string buffer_;
};

std::pair<std::string, std::string> parse_key_value_line(const std::string & line);
std::string cycle_mock_lines(const std::vector<std::string> & lines, std::size_t index);

}  // namespace medirun_sensor
