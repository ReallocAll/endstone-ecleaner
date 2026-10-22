// Exercise the actual staged publication path without starting a BDS host.
#include "../src/ecleaner.cpp"

#include <fstream>
#include <chrono>
#include <iostream>

struct TestTask : endstone::Task {
    bool cancelled = false;
    endstone::TaskId getTaskId() const override { return 7; }
    endstone::Plugin *getOwner() const override { return nullptr; }
    bool isSync() const override { return true; }
    bool isCancelled() const override { return cancelled; }
    void cancel() override { cancelled = true; }
};

struct ConfigTestAccess {
    static void initialize(ECleaner &plugin)
    {
        plugin.chunk_entity_guard_ = std::make_unique<ChunkEntityGuard>(plugin, [] { return std::nullopt; });
        plugin.falling_block_guard_ = std::make_unique<FallingBlockGuard>(plugin, [] { return std::nullopt; });
    }
    static void apply(ECleaner &plugin, json config, bool save = false)
    {
        plugin.apply_config(std::move(config), save);
    }
    static void markRuntime(ECleaner &plugin)
    {
        item_clean_task = std::make_shared<TestTask>();
        entity_clean_task = std::make_shared<TestTask>();
        plugin.chunk_entity_guard_->reconcile_task_ = std::make_shared<TestTask>();
        plugin.falling_block_guard_->control_task_ = std::make_shared<TestTask>();
        plugin.chunk_entity_guard_->pressure_active_ = true;
        plugin.falling_block_guard_->states_[nullptr].tokens = 17.0;
    }
    static bool preserved(const ECleaner &plugin)
    {
        const auto &chunk = *plugin.chunk_entity_guard_;
        const auto &falling = *plugin.falling_block_guard_;
        return item_clean_task && !item_clean_task->isCancelled()
               && entity_clean_task && !entity_clean_task->isCancelled()
               && chunk.reconcile_task_ && !chunk.reconcile_task_->isCancelled()
               && falling.control_task_ && !falling.control_task_->isCancelled()
               && chunk.pressure_active_ && chunk.protect_named_ && chunk.protection_enabled_
               && chunk.protect_tag_ == "ecleaner_protect"
               && chunk.protected_types_.contains("minecraft:wolf")
               && falling.states_.at(nullptr).tokens == 17.0 && !falling.reject_spawns_;
    }
};

static void require(bool condition, const char *message)
{
    if (!condition) throw std::runtime_error(message);
}

int main()
{
    const auto previous = std::filesystem::current_path();
    const auto directory = std::filesystem::temp_directory_path()
        / ("ecleaner_transaction_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(directory / "plugins/ecleaner/language");
    std::filesystem::current_path(directory);
    try {
        EndstonePluginImpl plugin;
        ConfigTestAccess::initialize(plugin);
        const json baseline = make_default_config();
        ConfigTestAccess::apply(plugin, baseline, true);
        ConfigTestAccess::markRuntime(plugin);
        const auto old_lookup = live_config.item_clean_id_lookup;
        const auto old_entity_lookup = live_config.entity_clean_lookup;
        const auto rejects = [&](json candidate, bool save = false) {
            bool failed = false;
            try { ConfigTestAccess::apply(plugin, std::move(candidate), save); }
            catch (const std::exception &) { failed = true; }
            require(failed, "invalid configuration accepted");
            require(!live_config.item_clean_whitelist && live_config.item_clean_id_lookup == old_lookup,
                    "failed publication changed item deletion policy");
            require(!live_config.entity_clean_whitelist && live_config.entity_clean_lookup == old_entity_lookup,
                    "failed publication changed entity deletion policy");
            require(live_config.item_clean_interval_seconds == 10 && live_config.entity_clean_interval_seconds == 60,
                    "failed publication changed scheduling intervals");
            require(ConfigTestAccess::preserved(plugin), "failed publication changed guard config or runtime state");
            require(ecleaner::config::readTomlFile(config_path) == baseline, "failed save changed original file");
        };
        for (const char *key : {"auto_item_clean", "auto_entity_clean", "item_clean_ids",
             "item_clean_legacy_names", "entity_clean_list", "entity_clean_whitelist",
             "item_clean_interval_seconds", "entity_clean_interval_seconds", "broadcast_cleanup_results", "language"}) {
            auto candidate = baseline;
            candidate["item_clean_whitelist"] = true;
            candidate[key] = json::object();
            rejects(candidate, true);
        }
        auto candidate = baseline;
        candidate["item_clean_whitelist"] = true;
        candidate["falling_block_guard"]["mode"] = "invalid";
        rejects(candidate, true);
        candidate = baseline;
        candidate["item_clean_whitelist"] = true;
        candidate["unknown_nested_array"] = json::array({json::array({1, 2})});
        rejects(candidate, true);
        {
            std::ofstream language("plugins/ecleaner/language/zh_CN.json");
            language << "{malformed";
        }
        candidate = baseline;
        candidate["item_clean_whitelist"] = true;
        rejects(candidate, true);
        {
            std::ofstream language("plugins/ecleaner/language/zh_CN.json");
            language << "{\"hello\": 42}";
        }
        rejects(candidate, true);
        std::filesystem::remove("plugins/ecleaner/language/zh_CN.json");

        auto oversized = baseline;
        oversized["item_clean_interval_seconds"] = std::numeric_limits<std::int64_t>::max();
        oversized["entity_clean_interval_seconds"] = std::numeric_limits<std::int64_t>::min();
        const auto bounded = ecleaner::CleanerConfig::parse(oversized);
        require(bounded.item_clean_interval_seconds == 3600 && bounded.entity_clean_interval_seconds == 0,
                "interval conversion overflowed before clamping");

        candidate = baseline;
        candidate["item_clean_whitelist"] = true;
        candidate["item_clean_ids"] = {"minecraft:sand"};
        candidate["item_clean_interval_seconds"] = 25;
        candidate["falling_block_guard"].erase("mode"); // existing config migrates safely
        candidate["falling_block_guard"]["enabled"] = false;
        candidate["falling_block_guard"]["burst_capacity"] = 47.0;
        ConfigTestAccess::apply(plugin, candidate, true);
        require(live_config.item_clean_whitelist && live_config.item_clean_id_lookup.contains("minecraft:sand")
                && !live_config.item_clean_id_lookup.contains("minecraft:stone")
                && live_config.item_clean_interval_seconds == 25 && live_config.preserve_falling_blocks,
                "successful publication was inconsistent");
        require(ecleaner::config::readTomlFile(config_path)["falling_block_guard"]["mode"] == "monitor",
                "legacy configuration did not migrate to non-destructive mode");
        const auto saved = ecleaner::config::readTomlFile(config_path);
        require(saved["falling_block_guard"]["enabled"] == false
                && saved["falling_block_guard"]["burst_capacity"] == 47.0,
                "mode migration overwrote existing custom settings");
    }
    catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        std::filesystem::current_path(previous);
        std::filesystem::remove_all(directory);
        return 1;
    }
    std::filesystem::current_path(previous);
    std::filesystem::remove_all(directory);
}
