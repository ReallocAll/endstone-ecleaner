#pragma once

#include <endstone/endstone.hpp>
#include <nlohmann/json.hpp>

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>

class FallingBlockGuard {
public:
    using MsptQuery = std::function<std::optional<double>()>;

    FallingBlockGuard(endstone::Plugin &plugin, MsptQuery mspt_query);

    static nlohmann::json defaultConfig();
    static bool normalizeRootConfig(nlohmann::json &root);

    void configure(const nlohmann::json &root);
    void start();
    void reschedule();
    void stop();

private:
    struct DimensionState {
        std::uint64_t attempted{};
        std::uint64_t accepted{};
        std::uint64_t rejected{};

        bool throttled{false};
        double allowed_rate_per_second{};
        double tokens{};
        int recovery_streak{};

        std::chrono::steady_clock::time_point last_refill{};
        std::chrono::steady_clock::time_point last_control{};

        double last_attempt_rate{};
        double last_accepted_rate{};
    };

    [[nodiscard]] static bool isFallingBlock(const endstone::Actor &actor);
    void onActorSpawn(endstone::ActorSpawnEvent &event);
    void controlTick();
    void refillTokens(DimensionState &state, std::chrono::steady_clock::time_point now);
    void setThrottle(endstone::Dimension &dimension, DimensionState &state, double rate,
                     double mspt, double attempted_rate, const char *reason);
    void clearThrottle(endstone::Dimension &dimension, DimensionState &state,
                       double mspt, double attempted_rate);
    void logAdjustment(const endstone::Dimension &dimension, const DimensionState &state,
                       double mspt, double attempted_rate, const char *reason) const;

    endstone::Plugin &plugin_;
    MsptQuery mspt_query_;
    std::shared_ptr<endstone::Task> control_task_;

    bool started_{false};
    bool enabled_{true};
    int control_interval_ticks_{20};

    double pressure_mspt_threshold_{50.0};
    double recovery_mspt_threshold_{45.0};
    double backoff_factor_{0.5};
    double recovery_factor_{2.0};
    int recovery_stable_intervals_{5};

    double min_rate_per_second_{16.0};
    double activation_rate_per_second_{16.0};
    double burst_capacity_{32.0};

    bool log_adjustments_{true};

    std::unordered_map<endstone::Dimension *, DimensionState> states_;
};
