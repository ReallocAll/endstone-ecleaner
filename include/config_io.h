#pragma once

#include <nlohmann/json.hpp>

#include <string>

namespace ecleaner::config {

using json = nlohmann::json;

[[nodiscard]] json readTomlFile(const std::string &path);
void writeTomlFile(const std::string &path, const json &value);

}  // namespace ecleaner::config
