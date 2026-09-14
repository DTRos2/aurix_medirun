#pragma once

#include <optional>
#include <string>
#include <vector>

namespace medirun_mission
{

struct JsonField
{
  std::string key;
  std::string value;
  bool raw{false};
};

std::string escape_json(const std::string & value);
std::optional<std::string> get_json_string(const std::string & json, const std::string & key);
std::optional<double> get_json_number(const std::string & json, const std::string & key);
std::optional<bool> get_json_bool(const std::string & json, const std::string & key);
std::string make_json_object(const std::vector<JsonField> & fields);

}  // namespace medirun_mission
