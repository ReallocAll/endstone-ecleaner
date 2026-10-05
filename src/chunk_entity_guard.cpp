#include "chunk_entity_guard.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <string>
#include <utility>

namespace {

using json = nlohmann::json;

const std::vector<std::string> kDefaultProtectedTypes = {
    "minecraft:villager",
    "minecraft:villager_v2",
    "minecraft:zombie_villager",
    "minecraft:zombie_villager_v2",
    "minecraft:allay",
    "minecraft:horse",
    "minecraft:donkey",
    "minecraft:mule",
    "minecraft:camel",
    "minecraft:llama",
    "minecraft:trader_llama",
    "minecraft:wolf",
    "minecraft:cat",
    "minecraft:parrot",
    "minecraft:sniffer",
    "minecraft:iron_golem",
    "minecraft:snow_golem",
    "minecraft:shulker",
};

const std::vector<std::string> kDefaultGuardCleanableTypes = {
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
    "minecraft:silverfish",
    "minecraft:endermite",
    "minecraft:zombie_pigman",
    "minecraft:zombified_piglin",
    "minecraft:phantom",
};

std::vector<std::string> sanitizeStringArray(const json &value)
{
    std::vector<std::string> result;
    if (!value.is_array()) {
        return result;
    }

    result.reserve(value.size());
    for (const auto &entry : value) {
        if (entry.is_string()) {
            const auto text = entry.get<std::string>();
            if (!text.empty()) {
                result.push_back(text);
            }
        }
    }
    return result;
}

int normalizedInteger(json &object, const char *key, int fallback, int minimum, int maximum, bool &changed)
{
    std::int64_t raw = fallback;
    if (object.contains(key) && object[key].is_number_integer()) {
        raw = object[key].get<std::int64_t>();
    }
    else {
        changed = true;
    }

    const auto clamped = std::clamp(
        raw,
        static_cast<std::int64_t>(minimum),
        static_cast<std::int64_t>(maximum)
    );
    const int value = static_cast<int>(clamped);
    if (!object.contains(key) || !object[key].is_number_integer() || raw != clamped) {
        object[key] = value;
        changed = true;
    }
    return value;
}

bool normalizedBoolean(json &object, const char *key, bool fallback, bool &changed)
{
    if (!object.contains(key) || !object[key].is_boolean()) {
        object[key] = fallback;
        changed = true;
    }
    return object[key].get<bool>();
}

std::uint64_t steadyMillis()
{
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()
        ).count()
    );
}

}  // namespace

ChunkEntityGuard::ChunkEntityGuard(endstone::Plugin &plugin) : plugin_(plugin) {}

nlohmann::json ChunkEntityGuard::defaultProtectionConfig()
{
    return {
        {"enabled", true},
        {"protect_named", true},
        {"protect_tag", "ecleaner_protect"},
        {"types", kDefaultProtectedTypes},
    };
}

nlohmann::json ChunkEntityGuard::defaultGuardConfig()
{
    return {
        {"enabled", true},
        {"reconcile_interval_ticks", 20},
        {"type_limits", {
            {"minecraft:slime", 48},
            {"minecraft:silverfish", 64},
            {"minecraft:magma_cube", 48},
        }},
        {"cleanable_types", kDefaultGuardCleanableTypes},
        {"cleanable_mob_limit_per_chunk", 96},
        {"total_mob_limit_per_chunk", 160},
        {"cleanable_mob_limit_3x3", 256},
        {"emergency_delete_protected_mobs", false},
        {"log_triggers", true},
        {"log_cooldown_seconds", 10},
    };
}

bool ChunkEntityGuard::normalizeRootConfig(nlohmann::json &root)
{
    bool changed = false;

    if (!root.contains("entity_protection") || !root["entity_protection"].is_object()) {
        root["entity_protection"] = defaultProtectionConfig();
        changed = true;
    }
    auto &protection = root["entity_protection"];
    normalizedBoolean(protection, "enabled", true, changed);
    normalizedBoolean(protection, "protect_named", true, changed);

    if (!protection.contains("protect_tag") || !protection["protect_tag"].is_string()) {
        protection["protect_tag"] = "ecleaner_protect";
        changed = true;
    }

    if (!protection.contains("types") || !protection["types"].is_array()) {
        protection["types"] = kDefaultProtectedTypes;
        changed = true;
    }
    else {
        const auto sanitized = sanitizeStringArray(protection["types"]);
        if (protection["types"] != sanitized) {
            protection["types"] = sanitized;
            changed = true;
        }
    }

    if (!root.contains("chunk_entity_guard") || !root["chunk_entity_guard"].is_object()) {
        root["chunk_entity_guard"] = defaultGuardConfig();
        changed = true;
    }
    auto &guard = root["chunk_entity_guard"];

    normalizedBoolean(guard, "enabled", true, changed);
    normalizedInteger(guard, "reconcile_interval_ticks", 20, 1, 1200, changed);
    normalizedInteger(guard, "cleanable_mob_limit_per_chunk", 96, 0, 100000, changed);
    normalizedInteger(guard, "total_mob_limit_per_chunk", 160, 0, 100000, changed);
    normalizedInteger(guard, "cleanable_mob_limit_3x3", 256, 0, 100000, changed);
    normalizedBoolean(guard, "emergency_delete_protected_mobs", false, changed);
    normalizedBoolean(guard, "log_triggers", true, changed);
    normalizedInteger(guard, "log_cooldown_seconds", 10, 0, 3600, changed);

    if (!guard.contains("cleanable_types") || !guard["cleanable_types"].is_array()) {
        guard["cleanable_types"] = kDefaultGuardCleanableTypes;
        changed = true;
    }
    else {
        const auto sanitized = sanitizeStringArray(guard["cleanable_types"]);
        if (guard["cleanable_types"] != sanitized) {
            guard["cleanable_types"] = sanitized;
            changed = true;
        }
    }

    if (!guard.contains("type_limits") || !guard["type_limits"].is_object()) {
        guard["type_limits"] = defaultGuardConfig()["type_limits"];
        changed = true;
    }
    else {
        auto &limits = guard["type_limits"];
        for (auto it = limits.begin(); it != limits.end();) {
            if (!it.value().is_number_integer()) {
                it = limits.erase(it);
                changed = true;
                continue;
            }

            const auto raw = it.value().get<std::int64_t>();
            const auto clamped = std::clamp<std::int64_t>(raw, 0, 100000);
            if (clamped != raw) {
                it.value() = static_cast<int>(clamped);
                changed = true;
            }
            ++it;
        }
    }

    return changed;
}

void ChunkEntityGuard::configure(const nlohmann::json &root)
{
    const auto &protection = root.at("entity_protection");
    protection_enabled_ = protection.value("enabled", true);
    protect_named_ = protection.value("protect_named", true);
    protect_tag_ = protection.value("protect_tag", std::string("ecleaner_protect"));

    protected_types_.clear();
    for (const auto &type : protection.value("types", kDefaultProtectedTypes)) {
        protected_types_.insert(type);
    }

    const auto &guard = root.at("chunk_entity_guard");
    enabled_ = guard.value("enabled", true);
    reconcile_interval_ticks_ = std::clamp(guard.value("reconcile_interval_ticks", 20), 1, 1200);
    cleanable_mob_limit_per_chunk_ =
        std::clamp(guard.value("cleanable_mob_limit_per_chunk", 96), 0, 100000);
    total_mob_limit_per_chunk_ =
        std::clamp(guard.value("total_mob_limit_per_chunk", 160), 0, 100000);
    cleanable_mob_limit_3x3_ =
        std::clamp(guard.value("cleanable_mob_limit_3x3", 256), 0, 100000);
    emergency_delete_protected_mobs_ = guard.value("emergency_delete_protected_mobs", false);
    log_triggers_ = guard.value("log_triggers", true);
    log_cooldown_seconds_ = std::clamp(guard.value("log_cooldown_seconds", 10), 0, 3600);

    cleanable_types_.clear();
    for (const auto &type : guard.value("cleanable_types", kDefaultGuardCleanableTypes)) {
        cleanable_types_.insert(type);
    }

    type_limits_.clear();
    if (guard.contains("type_limits") && guard["type_limits"].is_object()) {
        for (const auto &[type, raw] : guard["type_limits"].items()) {
            if (raw.is_number_integer()) {
                const int limit = std::clamp(raw.get<int>(), 0, 100000);
                if (limit > 0) {
                    type_limits_[type] = limit;
                }
            }
        }
    }

    chunk_counts_.clear();
    actor_states_.clear();
}

void ChunkEntityGuard::start()
{
    if (started_) {
        reschedule();
        return;
    }

    started_ = true;
    plugin_.registerEvent<endstone::ActorSpawnEvent>(
        [this](endstone::ActorSpawnEvent &event) { onActorSpawn(event); },
        endstone::EventPriority::Highest,
        true
    );
    plugin_.registerEvent<endstone::ActorRemoveEvent>(
        [this](endstone::ActorRemoveEvent &event) { onActorRemove(event); },
        endstone::EventPriority::Monitor
    );

    reschedule();

    if (enabled_) {
        plugin_.getLogger().info(
            "Chunk entity guard enabled: reconcile=" + std::to_string(reconcile_interval_ticks_)
            + "t, type limits=" + std::to_string(type_limits_.size())
            + ", chunk cleanable=" + std::to_string(cleanable_mob_limit_per_chunk_)
            + ", chunk total=" + std::to_string(total_mob_limit_per_chunk_)
            + ", 3x3 cleanable=" + std::to_string(cleanable_mob_limit_3x3_) + "."
        );
    }
    else {
        plugin_.getLogger().info("Chunk entity guard is disabled by configuration.");
    }
}

void ChunkEntityGuard::reschedule()
{
    if (reconcile_task_) {
        reconcile_task_->cancel();
        reconcile_task_.reset();
    }
    if (immediate_task_) {
        immediate_task_->cancel();
        immediate_task_.reset();
        immediate_reconcile_pending_ = false;
    }

    chunk_counts_.clear();
    actor_states_.clear();

    if (!started_ || !enabled_) {
        return;
    }

    reconcile_task_ = plugin_.getServer().getScheduler().runTaskTimer(
        plugin_,
        [this]() { reconcile(); },
        static_cast<std::uint64_t>(reconcile_interval_ticks_),
        static_cast<std::uint64_t>(reconcile_interval_ticks_)
    );
    requestImmediateReconcile();
}

void ChunkEntityGuard::stop()
{
    if (reconcile_task_) {
        reconcile_task_->cancel();
        reconcile_task_.reset();
    }
    if (immediate_task_) {
        immediate_task_->cancel();
        immediate_task_.reset();
    }
    started_ = false;
    enabled_ = false;
    immediate_reconcile_pending_ = false;
    chunk_counts_.clear();
    actor_states_.clear();
}

std::size_t ChunkEntityGuard::ChunkKeyHash::operator()(const ChunkKey &key) const noexcept
{
    std::size_t seed = std::hash<void *>{}(key.dimension);
    seed ^= std::hash<int>{}(key.x) + 0x9e3779b9U + (seed << 6U) + (seed >> 2U);
    seed ^= std::hash<int>{}(key.z) + 0x9e3779b9U + (seed << 6U) + (seed >> 2U);
    return seed;
}

int ChunkEntityGuard::chunkCoordinate(int block_coordinate) noexcept
{
    int quotient = block_coordinate / 16;
    if (block_coordinate % 16 < 0) {
        --quotient;
    }
    return quotient;
}

ChunkEntityGuard::ChunkKey ChunkEntityGuard::chunkKey(endstone::Actor &actor)
{
    const auto location = actor.getLocation();
    return {
        &actor.getDimension(),
        chunkCoordinate(location.getBlockX()),
        chunkCoordinate(location.getBlockZ()),
    };
}

bool ChunkEntityGuard::isCleanableType(const std::string &type) const
{
    return cleanable_types_.contains(type);
}

bool ChunkEntityGuard::hasProtectionTag(const endstone::Actor &actor) const
{
    if (protect_tag_.empty()) {
        return false;
    }

    const auto tags = actor.getScoreboardTags();
    return std::find(tags.begin(), tags.end(), protect_tag_) != tags.end();
}

bool ChunkEntityGuard::isProtected(const endstone::Actor &actor, bool emergency) const
{
    if (actor.asPlayer() != nullptr) {
        return true;
    }

    if (emergency && emergency_delete_protected_mobs_) {
        return false;
    }

    if (!protection_enabled_) {
        return false;
    }

    if (protected_types_.contains(actor.getType())) {
        return true;
    }
    if (protect_named_ && !actor.getNameTag().empty()) {
        return true;
    }
    return hasProtectionTag(actor);
}

int ChunkEntityGuard::areaCleanableCount(const ChunkKey &center) const
{
    int total = 0;
    for (int dx = -1; dx <= 1; ++dx) {
        for (int dz = -1; dz <= 1; ++dz) {
            const ChunkKey key{center.dimension, center.x + dx, center.z + dz};
            if (const auto it = chunk_counts_.find(key); it != chunk_counts_.end()) {
                total += it->second.cleanable_mobs;
            }
        }
    }
    return total;
}

bool ChunkEntityGuard::wouldRejectSpawn(
    endstone::Actor &actor,
    const ChunkKey &key,
    const std::string &type,
    bool cleanable
) const
{
    const auto counter_it = chunk_counts_.find(key);
    const ChunkCounter *counter = counter_it == chunk_counts_.end() ? nullptr : &counter_it->second;

    bool normal_limit_reached = false;
    if (const auto limit_it = type_limits_.find(type); limit_it != type_limits_.end()) {
        const int current = counter == nullptr ? 0
                                               : (counter->type_counts.contains(type)
                                                      ? counter->type_counts.at(type)
                                                      : 0);
        normal_limit_reached = current >= limit_it->second;
    }

    if (cleanable && cleanable_mob_limit_per_chunk_ > 0 && counter != nullptr
        && counter->cleanable_mobs >= cleanable_mob_limit_per_chunk_) {
        normal_limit_reached = true;
    }

    if (cleanable && cleanable_mob_limit_3x3_ > 0
        && areaCleanableCount(key) >= cleanable_mob_limit_3x3_) {
        normal_limit_reached = true;
    }

    const bool emergency_limit_reached =
        total_mob_limit_per_chunk_ > 0 && counter != nullptr
        && counter->total_mobs >= total_mob_limit_per_chunk_;

    if (emergency_limit_reached) {
        return !isProtected(actor, true);
    }
    return normal_limit_reached && !isProtected(actor, false);
}

void ChunkEntityGuard::addActorState(
    endstone::Actor &actor,
    const ChunkKey &key,
    const std::string &type,
    bool cleanable
)
{
    const auto runtime_id = actor.getRuntimeId();
    if (actor_states_.contains(runtime_id)) {
        removeActorState(runtime_id);
    }

    actor_states_.emplace(runtime_id, ActorState{key, type, cleanable});
    auto &counter = chunk_counts_[key];
    ++counter.total_mobs;
    if (cleanable) {
        ++counter.cleanable_mobs;
    }
    ++counter.type_counts[type];
}

void ChunkEntityGuard::removeActorState(std::uint64_t runtime_id)
{
    const auto state_it = actor_states_.find(runtime_id);
    if (state_it == actor_states_.end()) {
        return;
    }

    const ActorState state = state_it->second;
    actor_states_.erase(state_it);

    const auto chunk_it = chunk_counts_.find(state.chunk);
    if (chunk_it == chunk_counts_.end()) {
        return;
    }

    auto &counter = chunk_it->second;
    counter.total_mobs = std::max(0, counter.total_mobs - 1);
    if (state.cleanable) {
        counter.cleanable_mobs = std::max(0, counter.cleanable_mobs - 1);
    }

    if (const auto type_it = counter.type_counts.find(state.type); type_it != counter.type_counts.end()) {
        if (--type_it->second <= 0) {
            counter.type_counts.erase(type_it);
        }
    }

    if (counter.total_mobs <= 0) {
        chunk_counts_.erase(chunk_it);
    }
}

void ChunkEntityGuard::onActorSpawn(endstone::ActorSpawnEvent &event)
{
    if (!enabled_) {
        return;
    }

    auto &actor = event.getActor();
    if (actor.asPlayer() != nullptr || actor.asMob() == nullptr) {
        return;
    }

    const auto key = chunkKey(actor);
    const std::string type = actor.getType();
    const bool cleanable = isCleanableType(type);

    if (wouldRejectSpawn(actor, key, type, cleanable)) {
        event.cancel();
        requestImmediateReconcile();
        return;
    }

    addActorState(actor, key, type, cleanable);

    const auto &counter = chunk_counts_.at(key);
    bool tripped = false;

    if (const auto it = type_limits_.find(type); it != type_limits_.end()) {
        const auto count_it = counter.type_counts.find(type);
        tripped = count_it != counter.type_counts.end() && count_it->second >= it->second;
    }

    tripped = tripped
              || (cleanable && cleanable_mob_limit_per_chunk_ > 0
                  && counter.cleanable_mobs >= cleanable_mob_limit_per_chunk_)
              || (total_mob_limit_per_chunk_ > 0
                  && counter.total_mobs >= total_mob_limit_per_chunk_)
              || (cleanable && cleanable_mob_limit_3x3_ > 0
                  && areaCleanableCount(key) >= cleanable_mob_limit_3x3_);

    if (tripped) {
        requestImmediateReconcile();
    }
}

void ChunkEntityGuard::onActorRemove(endstone::ActorRemoveEvent &event)
{
    if (!enabled_) {
        return;
    }
    removeActorState(event.getActor().getRuntimeId());
}

void ChunkEntityGuard::requestImmediateReconcile()
{
    if (!started_ || !enabled_ || immediate_reconcile_pending_) {
        return;
    }

    immediate_reconcile_pending_ = true;
    immediate_task_ = plugin_.getServer().getScheduler().runTask(plugin_, [this]() {
        immediate_reconcile_pending_ = false;
        if (started_ && enabled_) {
            reconcile();
        }
    });
}

void ChunkEntityGuard::rebuildSnapshot(std::vector<ActorRecord> &records)
{
    std::unordered_map<ChunkKey, ChunkCounter, ChunkKeyHash> new_counts;
    std::unordered_map<std::uint64_t, ActorState> new_states;

    const auto actors = plugin_.getServer().getLevel()->getActors();
    records.clear();
    records.reserve(actors.size());
    new_states.reserve(actors.size());

    for (auto *actor : actors) {
        if (actor == nullptr || actor->asPlayer() != nullptr || actor->asMob() == nullptr) {
            continue;
        }

        const auto key = chunkKey(*actor);
        const std::string type = actor->getType();
        const bool cleanable = isCleanableType(type);

        records.push_back({actor, key, type, cleanable});
        new_states.emplace(actor->getRuntimeId(), ActorState{key, type, cleanable});

        auto &counter = new_counts[key];
        ++counter.total_mobs;
        if (cleanable) {
            ++counter.cleanable_mobs;
        }
        ++counter.type_counts[type];
    }

    chunk_counts_.swap(new_counts);
    actor_states_.swap(new_states);
}

std::vector<ChunkEntityGuard::Trigger> ChunkEntityGuard::selectAreaTriggers(
    std::vector<Trigger> candidates
) const
{
    std::sort(candidates.begin(), candidates.end(), [](const Trigger &left, const Trigger &right) {
        return left.observed > right.observed;
    });

    std::vector<Trigger> selected;
    for (const auto &candidate : candidates) {
        bool overlaps = false;
        for (const auto &existing : selected) {
            if (candidate.center.dimension == existing.center.dimension
                && std::abs(candidate.center.x - existing.center.x) <= 2
                && std::abs(candidate.center.z - existing.center.z) <= 2) {
                overlaps = true;
                break;
            }
        }
        if (!overlaps) {
            selected.push_back(candidate);
        }
    }
    return selected;
}

std::vector<ChunkEntityGuard::Trigger> ChunkEntityGuard::findTriggers() const
{
    std::vector<Trigger> triggers;

    for (const auto &[key, counter] : chunk_counts_) {
        if (total_mob_limit_per_chunk_ > 0 && counter.total_mobs >= total_mob_limit_per_chunk_) {
            triggers.push_back({
                TriggerKind::ChunkTotal,
                key,
                {},
                counter.total_mobs,
                total_mob_limit_per_chunk_,
            });
            continue;
        }

        if (cleanable_mob_limit_per_chunk_ > 0
            && counter.cleanable_mobs >= cleanable_mob_limit_per_chunk_) {
            triggers.push_back({
                TriggerKind::ChunkCleanable,
                key,
                {},
                counter.cleanable_mobs,
                cleanable_mob_limit_per_chunk_,
            });
            continue;
        }

        for (const auto &[type, limit] : type_limits_) {
            const auto count_it = counter.type_counts.find(type);
            if (count_it != counter.type_counts.end() && count_it->second >= limit) {
                triggers.push_back({
                    TriggerKind::TypeLimit,
                    key,
                    type,
                    count_it->second,
                    limit,
                });
            }
        }
    }

    if (cleanable_mob_limit_3x3_ <= 0) {
        return triggers;
    }

    std::unordered_set<ChunkKey, ChunkKeyHash> candidate_centers;
    candidate_centers.reserve(chunk_counts_.size() * 3);
    for (const auto &[key, counter] : chunk_counts_) {
        if (counter.cleanable_mobs <= 0) {
            continue;
        }
        for (int dx = -1; dx <= 1; ++dx) {
            for (int dz = -1; dz <= 1; ++dz) {
                candidate_centers.insert({key.dimension, key.x + dx, key.z + dz});
            }
        }
    }

    std::vector<Trigger> area_candidates;
    for (const auto &center : candidate_centers) {
        const int observed = areaCleanableCount(center);
        if (observed >= cleanable_mob_limit_3x3_) {
            area_candidates.push_back({
                TriggerKind::AreaCleanable,
                center,
                {},
                observed,
                cleanable_mob_limit_3x3_,
            });
        }
    }

    auto selected_areas = selectAreaTriggers(std::move(area_candidates));
    triggers.insert(triggers.end(), selected_areas.begin(), selected_areas.end());
    return triggers;
}

bool ChunkEntityGuard::shouldLog(const Trigger &trigger)
{
    if (!log_triggers_) {
        return false;
    }

    std::string key = trigger.center.dimension->getName() + ":"
                      + std::to_string(trigger.center.x) + ":"
                      + std::to_string(trigger.center.z) + ":"
                      + std::to_string(static_cast<int>(trigger.kind)) + ":"
                      + trigger.type;

    const std::uint64_t now = steadyMillis();
    const std::uint64_t cooldown = static_cast<std::uint64_t>(log_cooldown_seconds_) * 1000;
    if (const auto it = last_log_millis_.find(key); it != last_log_millis_.end()) {
        if (cooldown > 0 && now - it->second < cooldown) {
            return false;
        }
    }

    last_log_millis_[std::move(key)] = now;
    return true;
}

void ChunkEntityGuard::logTrigger(
    const Trigger &trigger,
    int candidates,
    int removed,
    int protected_count
)
{
    if (!shouldLog(trigger)) {
        return;
    }

    std::string reason;
    switch (trigger.kind) {
    case TriggerKind::TypeLimit:
        reason = "type_limit type=" + trigger.type;
        break;
    case TriggerKind::ChunkCleanable:
        reason = "chunk_cleanable_limit";
        break;
    case TriggerKind::ChunkTotal:
        reason = "chunk_total_emergency";
        break;
    case TriggerKind::AreaCleanable:
        reason = "area_3x3_cleanable_limit";
        break;
    }

    plugin_.getLogger().warning(
        "Chunk entity guard triggered: dimension=" + trigger.center.dimension->getName()
        + " chunk=(" + std::to_string(trigger.center.x) + "," + std::to_string(trigger.center.z)
        + ") reason=" + reason
        + " observed=" + std::to_string(trigger.observed)
        + " limit=" + std::to_string(trigger.limit)
        + " candidates=" + std::to_string(candidates)
        + " removed=" + std::to_string(removed)
        + " protected=" + std::to_string(protected_count)
    );
}

void ChunkEntityGuard::applyTriggers(
    const std::vector<Trigger> &triggers,
    const std::vector<ActorRecord> &records
)
{
    struct TriggerStats {
        const Trigger *trigger{};
        int candidates{};
        int protected_count{};
        std::vector<std::uint64_t> eligible_ids;
    };

    std::vector<TriggerStats> stats;
    stats.reserve(triggers.size());

    std::unordered_map<std::uint64_t, endstone::Actor *> victims;

    for (const auto &trigger : triggers) {
        TriggerStats current;
        current.trigger = &trigger;

        for (const auto &record : records) {
            bool matches = false;
            switch (trigger.kind) {
            case TriggerKind::TypeLimit:
                matches = record.chunk == trigger.center && record.type == trigger.type;
                break;
            case TriggerKind::ChunkCleanable:
                matches = record.chunk == trigger.center && record.cleanable;
                break;
            case TriggerKind::ChunkTotal:
                matches = record.chunk == trigger.center;
                break;
            case TriggerKind::AreaCleanable:
                matches = record.cleanable
                          && record.chunk.dimension == trigger.center.dimension
                          && std::abs(record.chunk.x - trigger.center.x) <= 1
                          && std::abs(record.chunk.z - trigger.center.z) <= 1;
                break;
            }

            if (!matches || record.actor == nullptr) {
                continue;
            }

            ++current.candidates;
            const bool emergency = trigger.kind == TriggerKind::ChunkTotal;
            if (isProtected(*record.actor, emergency)) {
                ++current.protected_count;
                continue;
            }

            const auto runtime_id = record.actor->getRuntimeId();
            current.eligible_ids.push_back(runtime_id);
            victims.emplace(runtime_id, record.actor);
        }

        stats.push_back(std::move(current));
    }

    std::unordered_set<std::uint64_t> removed_ids;
    removed_ids.reserve(victims.size());

    for (const auto &[runtime_id, actor] : victims) {
        if (actor == nullptr || !actor->isValid() || actor->isDead()) {
            continue;
        }
        actor->remove();
        removed_ids.insert(runtime_id);
    }

    for (const auto &entry : stats) {
        int removed = 0;
        for (const auto runtime_id : entry.eligible_ids) {
            if (removed_ids.contains(runtime_id)) {
                ++removed;
            }
        }
        logTrigger(*entry.trigger, entry.candidates, removed, entry.protected_count);
    }
}

void ChunkEntityGuard::reconcile()
{
    if (!enabled_) {
        return;
    }

    std::vector<ActorRecord> records;
    rebuildSnapshot(records);

    const auto triggers = findTriggers();
    if (!triggers.empty()) {
        applyTriggers(triggers, records);
    }
}
