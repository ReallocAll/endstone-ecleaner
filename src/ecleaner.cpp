//
// Created by yuhang on 2025/4/27.
// Refactored for high-frequency cleanup by ReallocAll.
//

#include "ecleaner.h"
#include "config_io.h"
#include "version.h"

#include <endstone_papi/placeholder_api.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

translate Tran;

const std::string data_path = "plugins/ecleaner";
const std::string config_path = "plugins/ecleaner/config.toml";

std::shared_ptr<endstone::Task> item_clean_task;
std::shared_ptr<endstone::Task> entity_clean_task;

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

namespace {

const std::vector<std::string> kDefaultItemCleanIds = {
    "minecraft:netherrack",
    "minecraft:cobblestone",
    "minecraft:cobbled_deepslate",
    "minecraft:stone",
    "minecraft:deepslate",
    "minecraft:dirt",
    "minecraft:grass_block",
    "minecraft:gravel",
    "minecraft:tuff",
    "minecraft:granite",
    "minecraft:diorite",
    "minecraft:andesite",
    "minecraft:calcite",
    "minecraft:basalt",
    "minecraft:blackstone",
    "minecraft:end_stone",
    "minecraft:sandstone",
    "minecraft:red_sandstone",
};

const std::vector<std::string> kDefaultEntityCleanList = {
    "minecraft:zombie",
    "minecraft:skeleton",
    "minecraft:creeper",
    "minecraft:spider",
    "minecraft:cave_spider",
    "minecraft:husk",
    "minecraft:drowned",
    "minecraft:stray",
    "minecraft:bogged",
    "minecraft:witch",
    "minecraft:slime",
    "minecraft:magma_cube",
    "minecraft:zombie_pigman",
    "minecraft:zombified_piglin",
    "minecraft:phantom",
};

std::unordered_set<std::string> item_clean_id_lookup;
std::unordered_set<std::string> item_clean_legacy_name_lookup;
std::unordered_set<std::string> entity_clean_lookup;

json make_default_config()
{
    return {
        {"language", "zh_CN"},
        {"auto_item_clean", true},
        {"auto_entity_clean", true},
        {"item_clean_interval_seconds", 10},
        {"entity_clean_interval_seconds", 60},
        {"broadcast_cleanup_results", false},
        {"mspt_threshold", 50.0},
        {"mspt_window", "10s"},
        {"mspt_statistic", "p95"},
        {"item_clean_whitelist", false},
        {"item_clean_ids", kDefaultItemCleanIds},
        {"item_clean_legacy_names", json::array()},
        {"entity_clean_whitelist", false},
        {"entity_clean_list", kDefaultEntityCleanList},
        {"entity_protection", ChunkEntityGuard::defaultProtectionConfig()},
        {"chunk_entity_guard", ChunkEntityGuard::defaultGuardConfig()},
        {"falling_block_guard", FallingBlockGuard::defaultConfig()},
    };
}

std::optional<std::size_t> mspt_statistic_index(std::string_view statistic);

bool normalize_config(json &config)
{
    const json defaults = make_default_config();
    bool changed = false;

    if (ChunkEntityGuard::normalizeRootConfig(config)) {
        changed = true;
    }
    if (FallingBlockGuard::normalizeRootConfig(config)) {
        changed = true;
    }

    for (const auto &[key, value] : defaults.items()) {
        if (!config.contains(key)) {
            config[key] = value;
            changed = true;
        }
    }

    if (!config["mspt_threshold"].is_number()) {
        config["mspt_threshold"] = 50.0;
        changed = true;
    }
    else {
        double threshold = config["mspt_threshold"].get<double>();
        if (!std::isfinite(threshold)) {
            threshold = 50.0;
        }
        threshold = std::clamp(threshold, 0.0, 10000.0);
        if (config["mspt_threshold"].get<double>() != threshold) {
            config["mspt_threshold"] = threshold;
            changed = true;
        }
    }

    if (!config["mspt_window"].is_string()
        || (config["mspt_window"] != "10s" && config["mspt_window"] != "1m")) {
        config["mspt_window"] = "10s";
        changed = true;
    }

    if (!config["mspt_statistic"].is_string()
        || !mspt_statistic_index(config["mspt_statistic"].get<std::string>()).has_value()) {
        config["mspt_statistic"] = "p95";
        changed = true;
    }

    return changed;
}

void rebuild_lookup_sets()
{
    item_clean_id_lookup.clear();
    item_clean_id_lookup.reserve(item_clean_ids.size());
    item_clean_id_lookup.insert(item_clean_ids.begin(), item_clean_ids.end());

    item_clean_legacy_name_lookup.clear();
    item_clean_legacy_name_lookup.reserve(item_clean_legacy_names.size());
    item_clean_legacy_name_lookup.insert(item_clean_legacy_names.begin(), item_clean_legacy_names.end());

    entity_clean_lookup.clear();
    entity_clean_lookup.reserve(entity_clean_list.size());
    entity_clean_lookup.insert(entity_clean_list.begin(), entity_clean_list.end());
}

bool should_clean(bool whitelist_mode, bool listed)
{
    return whitelist_mode ? !listed : listed;
}

std::string strip_minecraft_formatting(std::string_view input)
{
    std::string output;
    output.reserve(input.size());

    for (std::size_t i = 0; i < input.size();) {
        const auto byte = static_cast<unsigned char>(input[i]);
        if (byte == 0xC2 && i + 2 < input.size()
            && static_cast<unsigned char>(input[i + 1]) == 0xA7) {
            i += 3;  // UTF-8 section sign plus one formatting-code byte.
            continue;
        }
        output.push_back(input[i]);
        ++i;
    }

    return output;
}

std::optional<std::array<double, 4>> parse_spark_mspt(std::string_view formatted)
{
    const std::string plain = strip_minecraft_formatting(formatted);
    std::array<double, 4> values{};

    std::size_t start = 0;
    for (std::size_t index = 0; index < values.size(); ++index) {
        const std::size_t end = index + 1 == values.size() ? std::string::npos : plain.find('/', start);
        if (end == std::string::npos && index + 1 != values.size()) {
            return std::nullopt;
        }

        const std::string token = plain.substr(start, end == std::string::npos ? end : end - start);
        try {
            std::size_t consumed = 0;
            const double value = std::stod(token, &consumed);
            if (consumed != token.size() || !std::isfinite(value)) {
                return std::nullopt;
            }
            values[index] = value;
        }
        catch (const std::exception &) {
            return std::nullopt;
        }

        if (end == std::string::npos) {
            start = plain.size();
        }
        else {
            start = end + 1;
        }
    }

    if (start != plain.size()) {
        return std::nullopt;
    }

    return values;
}

std::optional<std::size_t> mspt_statistic_index(std::string_view statistic)
{
    if (statistic == "min") {
        return 0;
    }
    if (statistic == "median") {
        return 1;
    }
    if (statistic == "p95") {
        return 2;
    }
    if (statistic == "max") {
        return 3;
    }
    return std::nullopt;
}

}  // namespace

void ECleaner::datafile_check() const
{
    std::filesystem::create_directories(data_path);
    std::filesystem::create_directories(language_path);

    const json defaults = make_default_config();

    if (!std::filesystem::exists(config_path)) {
        try {
            ecleaner::config::writeTomlFile(config_path, defaults);
            getLogger().info("Created default ECleaner config.");
        }
        catch (const std::exception &e) {
            getLogger().error(std::string("Failed to create ECleaner config: ") + e.what());
        }
        return;
    }

    try {
        json loaded_config = ecleaner::config::readTomlFile(config_path);

        if (normalize_config(loaded_config)) {
            ecleaner::config::writeTomlFile(config_path, loaded_config);
            getLogger().info("Normalized ECleaner TOML config.");
        }
    }
    catch (const std::exception &e) {
        getLogger().error(std::string("Failed to validate ECleaner config: ") + e.what());
    }
}

json ECleaner::read_config() const
{
    try {
        return ecleaner::config::readTomlFile(config_path);
    }
    catch (const std::exception &e) {
        getLogger().error(std::string("Failed to read ECleaner config: ") + e.what());
        return {{"error", e.what()}};
    }
}

bool ECleaner::load_config()
{
    json config = read_config();
    if (config.contains("error")) {
        return false;
    }

    try {
        if (normalize_config(config)) {
            ecleaner::config::writeTomlFile(config_path, config);
            getLogger().info("Normalized ECleaner TOML config during reload.");
        }
        auto_item_clean = config.value("auto_item_clean", true);
        auto_entity_clean = config.value("auto_entity_clean", true);

        item_clean_whitelist = config.value("item_clean_whitelist", false);
        entity_clean_whitelist = config.value("entity_clean_whitelist", false);

        item_clean_ids = config.value("item_clean_ids", kDefaultItemCleanIds);
        item_clean_legacy_names =
            config.value("item_clean_legacy_names", std::vector<std::string>{});
        entity_clean_list = config.value("entity_clean_list", kDefaultEntityCleanList);

        item_clean_interval_seconds =
            std::clamp(config.value("item_clean_interval_seconds", 10), 0, 3600);
        entity_clean_interval_seconds =
            std::clamp(config.value("entity_clean_interval_seconds", 60), 0, 3600);
        broadcast_cleanup_results = config.value("broadcast_cleanup_results", false);
        mspt_threshold = std::clamp(config.value("mspt_threshold", 50.0), 0.0, 10000.0);
        mspt_window = config.value("mspt_window", std::string("10s"));
        mspt_statistic = config.value("mspt_statistic", std::string("p95"));

        rebuild_lookup_sets();
        if (chunk_entity_guard_) {
            chunk_entity_guard_->configure(config);
        }
        if (falling_block_guard_) {
            falling_block_guard_->configure(config);
        }

        const std::string language = config.value("language", std::string("zh_CN"));
        language_file = language_path + language + ".json";
        Tran = translate(language_file);
        Tran.loadLanguage();

        return true;
    }
    catch (const std::exception &e) {
        getLogger().error(std::string("Invalid ECleaner config: ") + e.what());
        return false;
    }
}

int ECleaner::clean_item() const
{
    int total_clean_num = 0;

    for (const auto &actor : getServer().getLevel()->getActors()) {
        auto *item = actor->asItem();
        if (item == nullptr) {
            continue;
        }

        const auto stack = item->getItemStack();
        const std::string item_id = static_cast<std::string>(stack.getType().getId());

        bool listed = item_clean_id_lookup.contains(item_id);
        if (!listed && !item_clean_legacy_name_lookup.empty()) {
            listed = item_clean_legacy_name_lookup.contains(actor->getName());
        }

        if (should_clean(item_clean_whitelist, listed)) {
            actor->remove();
            ++total_clean_num;
        }
    }

    return total_clean_num;
}

int ECleaner::clean_entity() const
{
    int total_clean_num = 0;

    for (const auto &actor : getServer().getLevel()->getActors()) {
        if (actor->asItem() != nullptr || actor->asPlayer() != nullptr) {
            continue;
        }

        // Shared high-value protection applies to both MSPT cleanup and the
        // chunk safety guard, so whitelist mode cannot accidentally remove
        // villagers, pets, named mobs, or explicitly protected actors.
        if (chunk_entity_guard_ && chunk_entity_guard_->isProtected(*actor)) {
            continue;
        }
        if (!chunk_entity_guard_ && !actor->getNameTag().empty()) {
            continue;
        }

        const std::string type = actor->getType();
        const bool listed = entity_clean_lookup.contains(type);
        if (should_clean(entity_clean_whitelist, listed)) {
            actor->remove();
            ++total_clean_num;
        }
    }

    return total_clean_num;
}

std::optional<double> ECleaner::query_mspt() const
{
    auto api = getServer().getServiceManager().load<papi::PlaceholderAPI>(
        std::string(papi::PlaceholderAPI::ServiceName)
    );
    if (!api || !api->isActive() || !api->isRegistered("spark")) {
        return std::nullopt;
    }

    const std::string placeholder =
        mspt_window == "1m" ? "{spark:tickduration_1m}" : "{spark:tickduration_10s}";
    const std::string resolved = api->setPlaceholders(nullptr, placeholder);
    if (resolved == placeholder) {
        return std::nullopt;
    }

    const auto values = parse_spark_mspt(resolved);
    const auto index = mspt_statistic_index(mspt_statistic);
    if (!values || !index) {
        return std::nullopt;
    }

    return (*values)[*index];
}

bool ECleaner::should_run_automatic_cleanup() const
{
    if (mspt_threshold <= 0.0) {
        return true;
    }

    const auto current_mspt = query_mspt();
    return current_mspt.has_value() && *current_mspt >= mspt_threshold;
}

void ECleaner::run_scheduled_item_clean() const
{
    if (getServer().getOnlinePlayers().empty() || !should_run_automatic_cleanup()) {
        return;
    }

    const int cleaned = clean_item();
    if (broadcast_cleanup_results && cleaned > 0) {
        getServer().broadcastMessage(
            "§l§2[ECleaner] §r§e" + Tran.getLocal("Number of dropped items cleaned up: ")
            + std::to_string(cleaned)
        );
    }
}

void ECleaner::run_scheduled_entity_clean() const
{
    if (getServer().getOnlinePlayers().empty() || !should_run_automatic_cleanup()) {
        return;
    }

    const int cleaned = clean_entity();
    if (broadcast_cleanup_results && cleaned > 0) {
        getServer().broadcastMessage(
            "§l§2[ECleaner] §r§e" + Tran.getLocal("Number of entities cleaned up: ")
            + std::to_string(cleaned)
        );
    }
}

void ECleaner::schedule_cleanup_tasks()
{
    if (item_clean_task) {
        item_clean_task->cancel();
        item_clean_task.reset();
    }
    if (entity_clean_task) {
        entity_clean_task->cancel();
        entity_clean_task.reset();
    }

    if (auto_item_clean && item_clean_interval_seconds > 0) {
        const auto interval_ticks = static_cast<std::uint64_t>(item_clean_interval_seconds) * 20;
        item_clean_task = getServer().getScheduler().runTaskTimer(
            *this,
            [this]() { run_scheduled_item_clean(); },
            interval_ticks,
            interval_ticks
        );
    }

    if (auto_entity_clean && entity_clean_interval_seconds > 0) {
        const auto interval_ticks = static_cast<std::uint64_t>(entity_clean_interval_seconds) * 20;
        entity_clean_task = getServer().getScheduler().runTaskTimer(
            *this,
            [this]() { run_scheduled_entity_clean(); },
            interval_ticks,
            interval_ticks
        );
    }
}

void ECleaner::onLoad()
{
    datafile_check();
}

void ECleaner::onEnable()
{
    chunk_entity_guard_ = std::make_unique<ChunkEntityGuard>(*this, [this]() { return query_mspt(); });
    falling_block_guard_ = std::make_unique<FallingBlockGuard>(*this, [this]() { return query_mspt(); });

    if (!load_config()) {
        getLogger().warning("ECleaner config could not be loaded; using in-memory defaults.");
        item_clean_ids = kDefaultItemCleanIds;
        item_clean_legacy_names.clear();
        entity_clean_list = kDefaultEntityCleanList;
        rebuild_lookup_sets();

        json defaults = make_default_config();
        ChunkEntityGuard::normalizeRootConfig(defaults);
        FallingBlockGuard::normalizeRootConfig(defaults);
        chunk_entity_guard_->configure(defaults);
        falling_block_guard_->configure(defaults);
    }

    schedule_cleanup_tasks();
    chunk_entity_guard_->start();
    falling_block_guard_->start();

    auto papi_api = getServer().getServiceManager().load<papi::PlaceholderAPI>(
        std::string(papi::PlaceholderAPI::ServiceName)
    );
    if (mspt_threshold > 0.0
        && (!papi_api || !papi_api->isActive() || !papi_api->isRegistered("spark"))) {
        getLogger().warning(
            "MSPT guard is enabled, but PAPI/Spark data is unavailable. "
            "Automatic cleanup will fail closed until {spark:tickduration_*} resolves."
        );
    }

    getLogger().info(
        "ECleaner " + getDescription().getVersion()
        + " enabled: item interval=" + std::to_string(item_clean_interval_seconds)
        + "s, entity interval=" + std::to_string(entity_clean_interval_seconds)
        + "s, mspt guard=" + std::to_string(mspt_threshold)
        + "ms (" + mspt_window + " " + mspt_statistic + ")"
        + ", chunk entity guard=loaded, falling block guard=loaded."
    );
}

void ECleaner::onDisable()
{
    if (chunk_entity_guard_) {
        chunk_entity_guard_->stop();
    }
    if (falling_block_guard_) {
        falling_block_guard_->stop();
    }

    if (item_clean_task) {
        item_clean_task->cancel();
        item_clean_task.reset();
    }
    if (entity_clean_task) {
        entity_clean_task->cancel();
        entity_clean_task.reset();
    }
}

bool ECleaner::onCommand(
    endstone::CommandSender &sender,
    const endstone::Command &command,
    const std::vector<std::string> &args
)
{
    if (command.getName() != "ecl") {
        return false;
    }

    if (args.empty()) {
        if (auto *player = sender.asPlayer()) {
            ecl_main_menu(*player);
        }
        else {
            sender.sendMessage("Usage: /ecl clean [item|entity] | /ecl reload");
        }
        return true;
    }

    if (args[0] == "reload") {
        if (!load_config()) {
            sender.sendErrorMessage("ECleaner config reload failed.");
            return true;
        }

        schedule_cleanup_tasks();
        if (chunk_entity_guard_) {
            chunk_entity_guard_->reschedule();
        }
        if (falling_block_guard_) {
            falling_block_guard_->reschedule();
        }
        sender.sendMessage(Tran.getLocal("Reload completed."));
        return true;
    }

    if (args[0] != "clean") {
        sender.sendErrorMessage("Usage: /ecl clean [item|entity] | /ecl reload");
        return true;
    }

    if (args.size() == 1) {
        int item_count = 0;
        int entity_count = 0;

        if (auto_item_clean) {
            item_count = clean_item();
        }
        if (auto_entity_clean) {
            entity_count = clean_entity();
        }

        sender.sendMessage(
            Tran.getLocal("Number of dropped items cleaned up: ") + std::to_string(item_count)
            + " | " + Tran.getLocal("Number of entities cleaned up: ")
            + std::to_string(entity_count)
        );
        return true;
    }

    if (args[1] == "item") {
        const int cleaned = clean_item();
        sender.sendMessage(
            Tran.getLocal("Number of dropped items cleaned up: ") + std::to_string(cleaned)
        );
        return true;
    }

    if (args[1] == "entity") {
        const int cleaned = clean_entity();
        sender.sendMessage(
            Tran.getLocal("Number of entities cleaned up: ") + std::to_string(cleaned)
        );
        return true;
    }

    sender.sendErrorMessage("Usage: /ecl clean [item|entity] | /ecl reload");
    return true;
}

void ECleaner::ecl_main_menu(endstone::Player &player)
{
    endstone::ModalForm menu;
    menu.setTitle(Tran.getLocal("ECL Config Menu"));

    endstone::Toggle auto_entity;
    auto_entity.setLabel(Tran.getLocal("Auto clean entity"));
    auto_entity.setDefaultValue(auto_entity_clean);

    endstone::Toggle auto_item;
    auto_item.setLabel(Tran.getLocal("Auto clean item"));
    auto_item.setDefaultValue(auto_item_clean);

    endstone::Toggle entity_whitelist;
    entity_whitelist.setLabel(Tran.getLocal("Entity whitelist mode"));
    entity_whitelist.setDefaultValue(entity_clean_whitelist);

    endstone::Toggle item_whitelist;
    item_whitelist.setLabel(Tran.getLocal("Item whitelist mode"));
    item_whitelist.setDefaultValue(item_clean_whitelist);

    endstone::Slider item_interval;
    item_interval.setLabel(Tran.getLocal("Item cleanup interval (seconds)"));
    item_interval.setMin(0);
    item_interval.setMax(300);
    item_interval.setStep(5);
    item_interval.setDefaultValue(static_cast<float>(item_clean_interval_seconds));

    endstone::Slider entity_interval;
    entity_interval.setLabel(Tran.getLocal("Entity cleanup interval (seconds)"));
    entity_interval.setMin(0);
    entity_interval.setMax(600);
    entity_interval.setStep(10);
    entity_interval.setDefaultValue(static_cast<float>(entity_clean_interval_seconds));

    endstone::Toggle broadcast_results;
    broadcast_results.setLabel(Tran.getLocal("Broadcast scheduled cleanup results"));
    broadcast_results.setDefaultValue(broadcast_cleanup_results);

    menu.setControls({
        auto_entity,
        auto_item,
        entity_whitelist,
        item_whitelist,
        item_interval,
        entity_interval,
        broadcast_results,
    });

    menu.setOnSubmit([this](endstone::Player *p, const std::string &response) {
        try {
            const json values = json::parse(response);
            json config = read_config();
            if (config.contains("error")) {
                p->sendErrorMessage("Failed to read ECleaner config.");
                return;
            }

            auto_entity_clean = values.at(0).get<bool>();
            auto_item_clean = values.at(1).get<bool>();
            entity_clean_whitelist = values.at(2).get<bool>();
            item_clean_whitelist = values.at(3).get<bool>();
            item_clean_interval_seconds = std::clamp(values.at(4).get<int>(), 0, 3600);
            entity_clean_interval_seconds = std::clamp(values.at(5).get<int>(), 0, 3600);
            broadcast_cleanup_results = values.at(6).get<bool>();

            config["auto_entity_clean"] = auto_entity_clean;
            config["auto_item_clean"] = auto_item_clean;
            config["entity_clean_whitelist"] = entity_clean_whitelist;
            config["item_clean_whitelist"] = item_clean_whitelist;
            config["item_clean_interval_seconds"] = item_clean_interval_seconds;
            config["entity_clean_interval_seconds"] = entity_clean_interval_seconds;
            config["broadcast_cleanup_results"] = broadcast_cleanup_results;
            ecleaner::config::writeTomlFile(config_path, config);
            schedule_cleanup_tasks();

            p->sendMessage(Tran.getLocal("Config file update over"));
        }
        catch (const std::exception &e) {
            p->sendErrorMessage(std::string("Failed to update ECleaner config: ") + e.what());
        }
    });

    player.sendForm(menu);
}

ENDSTONE_PLUGIN("ecleaner", ECLEANER_PLUGIN_VERSION, ECleaner)
{
    description = "High-frequency dropped-item and entity cleanup for Endstone";
    soft_depend = {"papi", "spark"};

    command("ecl")
        .description("ECleaner")
        .usages(
            "/ecl",
            "/ecl clean [entity|item]",
            "/ecl reload"
        )
        .permissions("ecleaner.command.op");

    permission("ecleaner.command.op")
        .description("ECleaner operator command")
        .default_(endstone::PermissionDefault::Operator);
}
