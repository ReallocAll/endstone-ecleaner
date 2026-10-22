#pragma once

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <nlohmann/json.hpp>
#include <string>
#include <unordered_set>
#include <vector>

namespace ecleaner {
// Construct the complete policy, including derived deletion lookups, before publication.
struct CleanerConfig {
    bool preserve_falling_blocks = true;
    bool auto_item_clean = true;
    bool auto_entity_clean = true;
    bool item_clean_whitelist = false;
    std::vector<std::string> item_clean_ids;
    std::vector<std::string> item_clean_legacy_names;
    bool entity_clean_whitelist = false;
    std::vector<std::string> entity_clean_list;
    int item_clean_interval_seconds = 10;
    int entity_clean_interval_seconds = 60;
    bool broadcast_cleanup_results = false;
    double mspt_threshold = 50.0;
    std::string mspt_window = "10s";
    std::string mspt_statistic = "p95";

    std::unordered_set<std::string> item_clean_id_lookup;
    std::unordered_set<std::string> item_clean_legacy_name_lookup;
    std::unordered_set<std::string> entity_clean_lookup;

    static CleanerConfig parse(const nlohmann::json &config)
    {
        CleanerConfig result;
        result.preserve_falling_blocks = config.at("falling_block_guard").at("mode") != "reject";
        config.at("auto_item_clean").get_to(result.auto_item_clean);
        config.at("auto_entity_clean").get_to(result.auto_entity_clean);
        config.at("item_clean_whitelist").get_to(result.item_clean_whitelist);
        config.at("item_clean_ids").get_to(result.item_clean_ids);
        config.at("item_clean_legacy_names").get_to(result.item_clean_legacy_names);
        config.at("entity_clean_whitelist").get_to(result.entity_clean_whitelist);
        config.at("entity_clean_list").get_to(result.entity_clean_list);
        result.item_clean_interval_seconds = interval(config.at("item_clean_interval_seconds"));
        result.entity_clean_interval_seconds = interval(config.at("entity_clean_interval_seconds"));
        config.at("broadcast_cleanup_results").get_to(result.broadcast_cleanup_results);
        config.at("mspt_threshold").get_to(result.mspt_threshold);
        config.at("mspt_window").get_to(result.mspt_window);
        config.at("mspt_statistic").get_to(result.mspt_statistic);
        result.item_clean_id_lookup.insert(result.item_clean_ids.begin(), result.item_clean_ids.end());
        result.item_clean_legacy_name_lookup.insert(result.item_clean_legacy_names.begin(), result.item_clean_legacy_names.end());
        result.entity_clean_lookup.insert(result.entity_clean_list.begin(), result.entity_clean_list.end());
        return result;
    }
private:
    static int interval(const nlohmann::json &value)
    {
        const double raw = value.get<double>();
        if (!std::isfinite(raw)) throw std::runtime_error("cleanup interval must be finite");
        // Clamp before narrowing, preserving the legacy fractional-seconds truncation.
        return static_cast<int>(std::clamp(raw, 0.0, 3600.0));
    }
};
}  // namespace ecleaner
