//
// Created by yuhang on 2025/4/27.
// Refactored for high-frequency cleanup by ReallocAll.
//

#include "ecleaner.h"
#include "version.h"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

translate Tran;

const std::string data_path = "plugins/ecleaner";
const std::string config_path = "plugins/ecleaner/config.json";

std::shared_ptr<endstone::Task> item_clean_task;
std::shared_ptr<endstone::Task> entity_clean_task;

bool auto_item_clean = true;
bool auto_entity_clean = true;
bool item_clean_whitelist = false;
std::vector<std::string> item_clean_list;
bool entity_clean_whitelist = false;
std::vector<std::string> entity_clean_list;
int item_clean_interval_seconds = 10;
int entity_clean_interval_seconds = 60;
bool broadcast_cleanup_results = false;

namespace {

const std::vector<std::string> kDefaultItemCleanList = {
    "Netherrack",
    "Cobblestone",
    "Cobbled Deepslate",
    "Stone",
    "Deepslate",
    "Dirt",
    "Grass Block",
    "Gravel",
    "Tuff",
    "Granite",
    "Diorite",
    "Andesite",
    "Calcite",
    "Basalt",
    "Blackstone",
    "End Stone",
    "Sandstone",
    "Red Sandstone",
};

const std::vector<std::string> kDefaultEntityCleanList = {
    "minecraft:zombie",
    "minecraft:skeleton",
    "minecraft:creeper",
    "minecraft:spider",
    "minecraft:husk",
    "minecraft:drowned",
    "minecraft:stray",
    "minecraft:bogged",
    "minecraft:phantom",
};

json make_default_config()
{
    return {
        {"language", "zh_CN"},
        {"auto_item_clean", true},
        {"auto_entity_clean", true},
        {"item_clean_interval_seconds", 10},
        {"entity_clean_interval_seconds", 60},
        {"broadcast_cleanup_results", false},
        {"item_clean_whitelist", false},
        {"item_clean_list", kDefaultItemCleanList},
        {"entity_clean_whitelist", false},
        {"entity_clean_list", kDefaultEntityCleanList},
    };
}

bool is_listed(const std::vector<std::string> &list, const std::string &value)
{
    return std::find(list.begin(), list.end(), value) != list.end();
}

bool should_clean(bool whitelist_mode, const std::vector<std::string> &list, const std::string &value)
{
    const bool listed = is_listed(list, value);
    return whitelist_mode ? !listed : listed;
}

void write_json_file(const std::string &path, const json &value)
{
    std::ofstream out(path);
    if (!out.is_open()) {
        throw std::runtime_error("failed to open config file for writing");
    }
    out << value.dump(4);
}

}  // namespace

void ECleaner::datafile_check() const
{
    std::filesystem::create_directories(data_path);
    std::filesystem::create_directories(language_path);

    const json defaults = make_default_config();

    if (!std::filesystem::exists(config_path)) {
        try {
            write_json_file(config_path, defaults);
            getLogger().info("Created default ECleaner config.");
        }
        catch (const std::exception &e) {
            getLogger().error(std::string("Failed to create ECleaner config: ") + e.what());
        }
        return;
    }

    try {
        std::ifstream file(config_path);
        json loaded_config;
        file >> loaded_config;

        bool changed = false;
        for (const auto &[key, value] : defaults.items()) {
            if (!loaded_config.contains(key)) {
                loaded_config[key] = value;
                changed = true;
            }
        }

        // Legacy 0.1.x keys are superseded by independent second-based schedules.
        if (loaded_config.erase("clean_time") > 0) {
            changed = true;
        }
        if (loaded_config.erase("clean_tps") > 0) {
            changed = true;
        }

        if (changed) {
            write_json_file(config_path, loaded_config);
            getLogger().info("Migrated ECleaner config to the high-frequency cleanup schema.");
        }
    }
    catch (const std::exception &e) {
        getLogger().error(std::string("Failed to validate ECleaner config: ") + e.what());
    }
}

json ECleaner::read_config() const
{
    try {
        std::ifstream file(config_path);
        if (!file.is_open()) {
            throw std::runtime_error("config file could not be opened");
        }

        json value;
        file >> value;
        return value;
    }
    catch (const std::exception &e) {
        getLogger().error(std::string("Failed to read ECleaner config: ") + e.what());
        return {{"error", e.what()}};
    }
}

bool ECleaner::load_config()
{
    const json config = read_config();
    if (config.contains("error")) {
        return false;
    }

    try {
        auto_item_clean = config.value("auto_item_clean", true);
        auto_entity_clean = config.value("auto_entity_clean", true);

        item_clean_whitelist = config.value("item_clean_whitelist", false);
        entity_clean_whitelist = config.value("entity_clean_whitelist", false);

        item_clean_list = config.value("item_clean_list", kDefaultItemCleanList);
        entity_clean_list = config.value("entity_clean_list", kDefaultEntityCleanList);

        item_clean_interval_seconds = std::clamp(config.value("item_clean_interval_seconds", 10), 0, 3600);
        entity_clean_interval_seconds = std::clamp(config.value("entity_clean_interval_seconds", 60), 0, 3600);
        broadcast_cleanup_results = config.value("broadcast_cleanup_results", false);

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
        if (actor->getType() != "minecraft:item") {
            continue;
        }

        if (should_clean(item_clean_whitelist, item_clean_list, actor->getName())) {
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
        if (actor->getType() == "minecraft:item") {
            continue;
        }

        // Named entities are assumed to be intentionally kept by players.
        if (!actor->getNameTag().empty()) {
            continue;
        }

        if (should_clean(entity_clean_whitelist, entity_clean_list, actor->getType())) {
            actor->remove();
            ++total_clean_num;
        }
    }

    return total_clean_num;
}

void ECleaner::run_scheduled_item_clean() const
{
    if (getServer().getOnlinePlayers().empty()) {
        return;
    }

    const int cleaned = clean_item();
    if (broadcast_cleanup_results && cleaned > 0) {
        getServer().broadcastMessage(
            "§l§2[ECleaner] §r§e" + Tran.getLocal("Number of dropped items cleaned up: ") + std::to_string(cleaned)
        );
    }
}

void ECleaner::run_scheduled_entity_clean() const
{
    if (getServer().getOnlinePlayers().empty()) {
        return;
    }

    const int cleaned = clean_entity();
    if (broadcast_cleanup_results && cleaned > 0) {
        getServer().broadcastMessage(
            "§l§2[ECleaner] §r§e" + Tran.getLocal("Number of entities cleaned up: ") + std::to_string(cleaned)
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
    if (!load_config()) {
        getLogger().warning("ECleaner config could not be loaded; using in-memory defaults.");
        item_clean_list = kDefaultItemCleanList;
        entity_clean_list = kDefaultEntityCleanList;
    }

    schedule_cleanup_tasks();

    getLogger().info(
        "ECleaner " + getServer().getPluginManager().getPlugin("ecleaner")->getDescription().getVersion()
        + " enabled: item interval=" + std::to_string(item_clean_interval_seconds)
        + "s, entity interval=" + std::to_string(entity_clean_interval_seconds) + "s."
    );
}

void ECleaner::onDisable()
{
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
            + " | " + Tran.getLocal("Number of entities cleaned up: ") + std::to_string(entity_count)
        );
        return true;
    }

    if (args[1] == "item") {
        const int cleaned = clean_item();
        sender.sendMessage(Tran.getLocal("Number of dropped items cleaned up: ") + std::to_string(cleaned));
        return true;
    }

    if (args[1] == "entity") {
        const int cleaned = clean_entity();
        sender.sendMessage(Tran.getLocal("Number of entities cleaned up: ") + std::to_string(cleaned));
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
            config.erase("clean_time");
            config.erase("clean_tps");

            write_json_file(config_path, config);
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
