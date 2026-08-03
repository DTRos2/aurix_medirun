#include "medirun_mission/json_utils.hpp"

#include <cctype>
#include <sstream>

namespace medirun_mission
{
namespace
{

std::optional<std::size_t> find_key_position(const std::string & json, const std::string & key)
{
  const std::string token = "\"" + key + "\"";
  const auto key_pos = json.find(token);
  if (key_pos == std::string::npos) {
    return std::nullopt;
  }
  const auto colon_pos = json.find(':', key_pos + token.size());
  if (colon_pos == std::string::npos) {
    return std::nullopt;
  }
  return colon_pos + 1;
}

std::size_t skip_ws(const std::string & json, std::size_t pos)
{
  while (pos < json.size() && std::isspace(static_cast<unsigned char>(json[pos])) != 0) {
    ++pos;
  }
  return pos;
}

}  // namespace

std::string escape_json(const std::string & value)
{
  std::string escaped;
  escaped.reserve(value.size());
  for (const char c : value) {
    switch (c) {
      case '\\':
        escaped += "\\\\";
        break;
      case '"':
        escaped += "\\\"";
        break;
      default:
        escaped += c;
        break;
    }
  }
  return escaped;
}

std::optional<std::string> get_json_string(const std::string & json, const std::string & key)
{
  const auto value_pos = find_key_position(json, key);
  if (!value_pos.has_value()) {
    return std::nullopt;
  }
  auto pos = skip_ws(json, *value_pos);
  if (pos >= json.size() || json[pos] != '"') {
    return std::nullopt;
  }
  ++pos;
  std::string value;
  bool escaped = false;
  for (; pos < json.size(); ++pos) {
    const char c = json[pos];
    if (escaped) {
      value += c;
      escaped = false;
      continue;
    }
    if (c == '\\') {
      escaped = true;
      continue;
    }
    if (c == '"') {
      return value;
    }
    value += c;
  }
  return std::nullopt;
}

std::optional<double> get_json_number(const std::string & json, const std::string & key)
{
  const auto value_pos = find_key_position(json, key);
  if (!value_pos.has_value()) {
    return std::nullopt;
  }
  auto pos = skip_ws(json, *value_pos);
  const auto end = json.find_first_of(",}", pos);
  if (end == std::string::npos) {
    return std::nullopt;
  }
  try {
    return std::stod(json.substr(pos, end - pos));
  } catch (...) {
    return std::nullopt;
  }
}

std::optional<bool> get_json_bool(const std::string & json, const std::string & key)
{
  const auto value_pos = find_key_position(json, key);
  if (!value_pos.has_value()) {
    return std::nullopt;
  }
  const auto pos = skip_ws(json, *value_pos);
  if (json.compare(pos, 4, "true") == 0) {
    return true;
  }
  if (json.compare(pos, 5, "false") == 0) {
    return false;
  }
  return std::nullopt;
}

std::string make_json_object(const std::vector<JsonField> & fields)
{
  std::ostringstream out;
  out << "{";
  bool first = true;
  for (const auto & field : fields) {
    if (!first) {
      out << ",";
    }
    first = false;
    out << "\"" << escape_json(field.key) << "\":";
    if (field.raw) {
      out << field.value;
    } else {
      out << "\"" << escape_json(field.value) << "\"";
    }
  }
  out << "}";
  return out.str();
}

}  // namespace medirun_mission
