//
// Created by yuhang on 2025/4/27.
// Refactored for high-frequency cleanup by ReallocAll.
//

#pragma once

#include <endstone/endstone.hpp>
#include <endstone/plugin/plugin.h>
#include <nlohmann/json.hpp>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "chunk_entity_guard.h"
#include "translate.hpp"

using json = nlohmann::json;

extern translate Tran;

extern const std::string data_path;
extern const std::string config_path;

extern std::shared_ptr<endstone::Task> item_clean_task;
extern std::shared_ptr<endstone::Task> entity_clean_task;

extern bool auto_item_clean;
extern bool auto_entity_clean;
extern bool item_clean_whitelist;
extern std::vector<std::string> item_clean_ids;
extern std::vector<std::string> item_clean_legacy_names;
extern bool entity_clean_whitelist;
extern std::vector<std::string> entity_clean_list;
extern int item_clean_interval_seconds;
extern int entity_clean_interval_seconds;
extern bool broadcast_cleanup_results;
extern double mspt_threshold;
extern std::string mspt_window;
extern std::string mspt_statistic;

class ECleaner : public endstone::Plugin {
public:
    void datafile_check() const;

    [[nodiscard]] json read_config() const;
    bool load_config();
    void schedule_cleanup_tasks();

    [[nodiscard]] int clean_item() const;
    [[nodiscard]] int clean_entity() const;

    [[nodiscard]] std::optional<double> query_mspt() const;
    [[nodiscard]] bool should_run_automatic_cleanup() const;

    void run_scheduled_item_clean() const;
    void run_scheduled_entity_clean() const;

    void onLoad() override;
    void onEnable() override;
    void onDisable() override;

    bool onCommand(
        endstone::CommandSender &sender,
        const endstone::Command &command,
        const std::vector<std::string> &args
    ) override;

    void ecl_main_menu(endstone::Player &player);

private:
    std::unique_ptr<ChunkEntityGuard> chunk_entity_guard_;
};
