#include "config_io.h"

#include <toml++/toml.hpp>

#include <cstdint>
#include <atomic>
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <system_error>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <cerrno>
#include <fcntl.h>
#include <unistd.h>
#endif
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

    if (const auto *value = node.as_string()) {
        return value->get();
    }
    if (const auto *value = node.as_integer()) {
        return value->get();
    }
    if (const auto *value = node.as_floating_point()) {
        return value->get();
    }
    if (const auto *value = node.as_boolean()) {
        return value->get();
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

    // Serialization and syntax validation must finish before opening any output.
    std::ostringstream out;
    writeScalarEntries(out, value);
    writeTables(out, value, {});
    const auto bytes = out.str();
    const auto parsed = toml::parse(bytes);
    if (nodeToJson(parsed) != value) {
        throw std::runtime_error("TOML serialization would change configuration values");
    }

    const std::filesystem::path destination(path);
    static std::atomic<std::uint64_t> sequence{0};
    std::filesystem::path temporary;
#ifdef _WIN32
    HANDLE file = INVALID_HANDLE_VALUE;
#else
    int file = -1;
#endif
    for (int attempt = 0; attempt < 100; ++attempt) {
        temporary = destination;
        temporary += ".tmp." + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count())
            + "." + std::to_string(sequence.fetch_add(1));
#ifdef _WIN32
        file = CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW,
                           FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file != INVALID_HANDLE_VALUE) break;
        if (GetLastError() != ERROR_FILE_EXISTS && GetLastError() != ERROR_ALREADY_EXISTS)
            throw std::runtime_error("failed to create temporary TOML config");
#else
        file = ::open(temporary.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0600);
        if (file != -1) break;
        if (errno != EEXIST)
            throw std::system_error(errno, std::generic_category(), "create temporary TOML config");
#endif
    }
#ifdef _WIN32
    if (file == INVALID_HANDLE_VALUE) throw std::runtime_error("temporary TOML filename collision");
#else
    if (file == -1) throw std::runtime_error("temporary TOML filename collision");
#endif
    try {
        std::size_t offset = 0;
        while (offset < bytes.size()) {
#ifdef _WIN32
            DWORD written = 0;
            const auto count = static_cast<DWORD>(std::min<std::size_t>(bytes.size() - offset, 1 << 20));
            if (!WriteFile(file, bytes.data() + offset, count, &written, nullptr) || written == 0)
                throw std::runtime_error("failed to write temporary TOML config");
#else
            const auto written = ::write(file, bytes.data() + offset, bytes.size() - offset);
            if (written < 0 && errno == EINTR) continue;
            if (written <= 0)
                throw std::system_error(errno, std::generic_category(), "write temporary TOML config");
#endif
            offset += static_cast<std::size_t>(written);
        }
#ifdef _WIN32
        if (!FlushFileBuffers(file)) throw std::runtime_error("failed to flush temporary TOML config");
        const bool closed = CloseHandle(file);
        file = INVALID_HANDLE_VALUE;
        if (!closed) throw std::runtime_error("failed to close temporary TOML config");
        if (!MoveFileExW(temporary.c_str(), destination.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
            throw std::runtime_error("failed to replace TOML config");
#else
        if (::fsync(file) != 0)
            throw std::system_error(errno, std::generic_category(), "flush temporary TOML config");
        const int closed = ::close(file);
        file = -1;
        if (closed != 0)
            throw std::system_error(errno, std::generic_category(), "close temporary TOML config");
        std::filesystem::rename(temporary, destination);
#endif
    }
    catch (...) {
#ifdef _WIN32
        if (file != INVALID_HANDLE_VALUE) CloseHandle(file);
#else
        if (file != -1) ::close(file);
#endif
        std::error_code ignored;
        std::filesystem::remove(temporary, ignored);
        throw;
    }
}

}  // namespace ecleaner::config
