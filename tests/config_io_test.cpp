#include "config_io.h"

#include <filesystem>
#include <iostream>
#include <fstream>
#include <chrono>
#include <limits>

int main()
{
    using ecleaner::config::json;

    const json expected = {
        {"enabled", true},
        {"integer_value", 42},
        {"floating_value", 50.0},
        {"string_value", "minecraft:falling_block"},
        {"empty_array", json::array()},
        {"string_array", {"minecraft:slime", "minecraft:silverfish"}},
        {"entity_protection", {
            {"enabled", true},
            {"protect_tag", "ecleaner_protect"},
        }},
        {"chunk_entity_guard", {
            {"pressure_mspt_threshold", 50.0},
            {"pressure_type_limits", {
                {"minecraft:slime", 96},
                {"minecraft:silverfish", 128},
            }},
        }},
    };

    const auto path =
        std::filesystem::temp_directory_path() / ("ecleaner_config_io_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".toml");

    try {
        ecleaner::config::writeTomlFile(path.string(), expected);
        const json actual = ecleaner::config::readTomlFile(path.string());
        if (actual != expected) {
            std::cerr << "TOML round-trip mismatch\n"
                      << "expected: " << expected.dump() << '\n'
                      << "actual:   " << actual.dump() << '\n';
            return 1;
        }
        for (const auto &bad : {json{{"nested", json::array({json::array({1})})}},
                                json{{"table_array", json::array({json{{"value", 1}}})}},
                                json{{"nonfinite", std::numeric_limits<double>::infinity()}}}) {
            bool rejected = false;
            try { ecleaner::config::writeTomlFile(path.string(), bad); }
            catch (const std::exception &) { rejected = true; }
            if (!rejected || ecleaner::config::readTomlFile(path.string()) != expected) {
                throw std::runtime_error("serialization failure damaged the existing config");
            }
        }
        // Real replacement failure: a nonempty directory cannot become a config file.
        const auto directory = path.string() + ".directory";
        std::filesystem::create_directory(directory);
        { std::ofstream marker(std::filesystem::path(directory) / "keep"); marker << "original"; }
        bool rejected = false;
        try { ecleaner::config::writeTomlFile(directory, expected); }
        catch (const std::exception &) { rejected = true; }
        if (!rejected || !std::filesystem::exists(std::filesystem::path(directory) / "keep")) {
            throw std::runtime_error("replacement failure damaged the existing target");
        }
        for (const auto &entry : std::filesystem::directory_iterator(path.parent_path())) {
            if (entry.path().filename().string().starts_with(path.filename().string() + ".directory.tmp.")) {
                throw std::runtime_error("replacement failure left a temporary file");
            }
        }
        std::filesystem::remove_all(directory);
        ecleaner::config::writeTomlFile(path.string(), json{{"replacement", true}});
        if (ecleaner::config::readTomlFile(path.string()) != json{{"replacement", true}}) {
            throw std::runtime_error("atomic replacement did not publish new config");
        }
        std::filesystem::remove(path);
    }
    catch (const std::exception &e) {
        std::filesystem::remove(path);
        std::cerr << "TOML round-trip failed: " << e.what() << '\n';
        return 1;
    }

    return 0;
}
