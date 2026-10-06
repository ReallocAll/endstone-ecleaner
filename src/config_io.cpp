#include "config_io.h"

#include <toml++/toml.hpp>

#include <cstdint>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace ecleaner::config {
namespace {

json nodeToJson(const toml::node &node)
{
    if (const auto *table = node.as_table()) {
        json result = json::object();
        for (const auto &[key, child] : *table) {
            result[std::string(key.str())] = nodeToJson(child);
        }
        return result;
    }

    if (const auto *array = node.as_array()) {
        json result = json::array();
        for (const auto &child : *array) {
            result.push_back(nodeToJson(child));
        }
        return result;
    }

    if (const auto value = node.value<std::string>()) {
        return *value;
    }
    if (const auto value = node.value<std::int64_t>()) {
        return *value;
    }
    if (const auto value = node.value<double>()) {
        return *value;
    }
    if (const auto value = node.value<bool>()) {
        return *value;
    }

    throw std::runtime_error("unsupported TOML value type");
}

bool isBareKey(std::string_view key)
{
    if (key.empty()) {
        return false;
    }
    for (const char ch : key) {
        const bool valid =
            (ch >= 'a' && ch <= 'z')
            || (ch >= 'A' && ch <= 'Z')
            || (ch >= '0' && ch <= '9')
            || ch == '_'
            || ch == '-';
        if (!valid) {
            return false;
        }
    }
    return true;
}

std::string quotedString(std::string_view value)
{
    return json(std::string(value)).dump();
}

std::string formatKey(std::string_view key)
{
    return isBareKey(key) ? std::string(key) : quotedString(key);
}

std::string formatNumber(double value)
{
    std::ostringstream stream;
    stream << std::setprecision(std::numeric_limits<double>::max_digits10) << value;
    std::string result = stream.str();
    if (result.find('.') == std::string::npos
        && result.find('e') == std::string::npos
        && result.find('E') == std::string::npos) {
        result += ".0";
    }
    return result;
}

std::string formatValue(const json &value)
{
    if (value.is_string()) {
        return quotedString(value.get_ref<const std::string &>());
    }
    if (value.is_boolean()) {
        return value.get<bool>() ? "true" : "false";
    }
    if (value.is_number_integer()) {
        return std::to_string(value.get<std::int64_t>());
    }
    if (value.is_number_unsigned()) {
        return std::to_string(value.get<std::uint64_t>());
    }
    if (value.is_number_float()) {
        return formatNumber(value.get<double>());
    }
    if (value.is_array()) {
        if (value.empty()) {
            return "[]";
        }

        std::ostringstream stream;
        stream << "[\n";
        for (const auto &entry : value) {
            if (entry.is_object() || entry.is_array()) {
                throw std::runtime_error("nested TOML arrays are not supported in ECleaner config");
            }
            stream << "    " << formatValue(entry) << ",\n";
        }
        stream << "]";
        return stream.str();
    }

    throw std::runtime_error("unsupported JSON value for TOML serialization");
}

std::string formatTablePath(const std::vector<std::string> &path)
{
    std::ostringstream stream;
    for (std::size_t i = 0; i < path.size(); ++i) {
        if (i > 0) {
            stream << '.';
        }
        stream << formatKey(path[i]);
    }
    return stream.str();
}

void writeScalarEntries(std::ostream &out, const json &object)
{
    for (const auto &[key, value] : object.items()) {
        if (value.is_object()) {
            continue;
        }
        out << formatKey(key) << " = " << formatValue(value) << '\n';
    }
}

void writeTables(
    std::ostream &out,
    const json &object,
    std::vector<std::string> path
)
{
    for (const auto &[key, value] : object.items()) {
        if (!value.is_object()) {
            continue;
        }

        path.push_back(key);
        out << '\n' << '[' << formatTablePath(path) << "]\n";
        writeScalarEntries(out, value);
        writeTables(out, value, path);
        path.pop_back();
    }
}

}  // namespace

json readTomlFile(const std::string &path)
{
    const toml::table table = toml::parse_file(path);
    json result = json::object();

    for (const auto &[key, node] : table) {
        result[std::string(key.str())] = nodeToJson(node);
    }
    return result;
}

void writeTomlFile(const std::string &path, const json &value)
{
    if (!value.is_object()) {
        throw std::runtime_error("ECleaner root config must be a TOML table");
    }

    std::ofstream out(path, std::ios::trunc);
    if (!out.is_open()) {
        throw std::runtime_error("failed to open TOML config file for writing");
    }

    writeScalarEntries(out, value);
    writeTables(out, value, {});

    if (!out.good()) {
        throw std::runtime_error("failed while writing TOML config file");
    }
}

}  // namespace ecleaner::config
