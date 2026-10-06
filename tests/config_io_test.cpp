#include "config_io.h"

#include <filesystem>
#include <iostream>

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
        std::filesystem::temp_directory_path() / "ecleaner_config_io_smoke.toml";

    try {
        ecleaner::config::writeTomlFile(path.string(), expected);
        const json actual = ecleaner::config::readTomlFile(path.string());
        std::filesystem::remove(path);

        if (actual != expected) {
            std::cerr << "TOML round-trip mismatch\n"
                      << "expected: " << expected.dump() << '\n'
                      << "actual:   " << actual.dump() << '\n';
            return 1;
        }
    }
    catch (const std::exception &e) {
        std::filesystem::remove(path);
        std::cerr << "TOML round-trip failed: " << e.what() << '\n';
        return 1;
    }

    return 0;
}
