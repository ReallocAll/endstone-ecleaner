#pragma once

#include <endstone/endstone.hpp>
#include <nlohmann/json.hpp>

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class ChunkEntityGuard {
public:
    explicit ChunkEntityGuard(endstone::Plugin &plugin);

    static nlohmann::json defaultProtectionConfig();
    static nlohmann::json defaultGuardConfig();
    static bool normalizeRootConfig(nlohmann::json &root);

    void configure(const nlohmann::json &root);
    void start();
    void reschedule();
    void stop();

    [[nodiscard]] bool isProtected(const endstone::Actor &actor, bool emergency = false) const;

private:
    struct ChunkKey {
        endstone::Dimension *dimension{};
        int x{};
        int z{};

        bool operator==(const ChunkKey &other) const noexcept
        {
            return dimension == other.dimension && x == other.x && z == other.z;
        }
    };

    struct ChunkKeyHash {
        std::size_t operator()(const ChunkKey &key) const noexcept;
    };

    struct ActorState {
        ChunkKey chunk;
        std::string type;
        bool cleanable{};
    };

    struct ChunkCounter {
        int total_mobs{};
        int cleanable_mobs{};
        std::unordered_map<std::string, int> type_counts;
    };

    struct ActorRecord {
        endstone::Actor *actor{};
        ChunkKey chunk;
        std::string type;
        bool cleanable{};
    };

    enum class TriggerKind {
        TypeLimit,
        ChunkCleanable,
        ChunkTotal,
        AreaCleanable,
    };

    struct Trigger {
        TriggerKind kind{};
        ChunkKey center;
        std::string type;
        int observed{};
        int limit{};
    };

    [[nodiscard]] static int chunkCoordinate(int block_coordinate) noexcept;
    [[nodiscard]] static ChunkKey chunkKey(endstone::Actor &actor);
    [[nodiscard]] bool isCleanableType(const std::string &type) const;
    [[nodiscard]] bool hasProtectionTag(const endstone::Actor &actor) const;
    [[nodiscard]] int areaCleanableCount(const ChunkKey &center) const;
    [[nodiscard]] bool wouldRejectSpawn(endstone::Actor &actor, const ChunkKey &key,
                                        const std::string &type, bool cleanable) const;

    void onActorSpawn(endstone::ActorSpawnEvent &event);
    void onActorRemove(endstone::ActorRemoveEvent &event);
    void addActorState(endstone::Actor &actor, const ChunkKey &key, const std::string &type, bool cleanable);
    void removeActorState(std::uint64_t runtime_id);
    void requestImmediateReconcile();
    void reconcile();
    void rebuildSnapshot(std::vector<ActorRecord> &records);
    [[nodiscard]] std::vector<Trigger> findTriggers() const;
    [[nodiscard]] std::vector<Trigger> selectAreaTriggers(std::vector<Trigger> candidates) const;
    void applyTriggers(const std::vector<Trigger> &triggers, const std::vector<ActorRecord> &records);
    void logTrigger(const Trigger &trigger, int candidates, int removed, int protected_count);
    [[nodiscard]] bool shouldLog(const Trigger &trigger);

    endstone::Plugin &plugin_;
    std::shared_ptr<endstone::Task> reconcile_task_;
    bool started_{false};
    bool immediate_reconcile_pending_{false};

    bool enabled_{true};
    int reconcile_interval_ticks_{20};
    std::unordered_map<std::string, int> type_limits_;
    std::unordered_set<std::string> cleanable_types_;
    int cleanable_mob_limit_per_chunk_{96};
    int total_mob_limit_per_chunk_{160};
    int cleanable_mob_limit_3x3_{256};
    bool emergency_delete_protected_mobs_{false};
    bool log_triggers_{true};
    int log_cooldown_seconds_{10};

    bool protection_enabled_{true};
    bool protect_named_{true};
    std::string protect_tag_{"ecleaner_protect"};
    std::unordered_set<std::string> protected_types_;

    std::unordered_map<ChunkKey, ChunkCounter, ChunkKeyHash> chunk_counts_;
    std::unordered_map<std::uint64_t, ActorState> actor_states_;
    std::unordered_map<std::string, std::uint64_t> last_log_generation_;
    std::uint64_t reconcile_generation_{0};
};
