#include "falling_block_guard.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <utility>

namespace {

using json = nlohmann::json;
using Clock = std::chrono::steady_clock;

double normalizedDouble(
    json &object,
    const char *key,
    double fallback,
    double minimum,
    double maximum,
    bool &changed
)
{
    double raw = fallback;
    if (object.contains(key) && object[key].is_number()) {
        raw = object[key].get<double>();
    }
    else {
        changed = true;
    }

    if (!std::isfinite(raw)) {
        raw = fallback;
        changed = true;
    }

    const double value = std::clamp(raw, minimum, maximum);
    if (!object.contains(key) || !object[key].is_number() || object[key].get<double>() != value) {
        object[key] = value;
        changed = true;
    }
    return value;
}

int normalizedInteger(
    json &object,
    const char *key,
    int fallback,
    int minimum,
    int maximum,
    bool &changed
)
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

}  // namespace

FallingBlockGuard::FallingBlockGuard(endstone::Plugin &plugin, MsptQuery mspt_query)
    : plugin_(plugin), mspt_query_(std::move(mspt_query))
{
}

nlohmann::json FallingBlockGuard::defaultConfig()
{
    return {
        {"enabled", true},
        {"control_interval_ticks", 20},
        {"pressure_mspt_threshold", 50.0},
        {"recovery_mspt_threshold", 45.0},
        {"backoff_factor", 0.5},
        {"recovery_factor", 2.0},
        {"recovery_stable_intervals", 5},
        {"min_rate_per_second", 16.0},
        {"activation_rate_per_second", 8.0},
        {"burst_capacity", 32.0},
        {"log_adjustments", true},
    };
}

bool FallingBlockGuard::normalizeRootConfig(nlohmann::json &root)
{
    bool changed = false;

    if (!root.contains("falling_block_guard") || !root["falling_block_guard"].is_object()) {
        root["falling_block_guard"] = defaultConfig();
        return true;
    }

    auto &guard = root["falling_block_guard"];
    normalizedBoolean(guard, "enabled", true, changed);
    normalizedInteger(guard, "control_interval_ticks", 20, 1, 1200, changed);

    const double pressure =
        normalizedDouble(guard, "pressure_mspt_threshold", 50.0, 1.0, 10000.0, changed);
    double recovery =
        normalizedDouble(guard, "recovery_mspt_threshold", 45.0, 0.0, 10000.0, changed);
    if (recovery >= pressure) {
        recovery = std::max(0.0, pressure - 5.0);
        guard["recovery_mspt_threshold"] = recovery;
        changed = true;
    }

    normalizedDouble(guard, "backoff_factor", 0.5, 0.05, 0.95, changed);
    normalizedDouble(guard, "recovery_factor", 2.0, 1.01, 10.0, changed);
    normalizedInteger(guard, "recovery_stable_intervals", 5, 1, 600, changed);
    normalizedDouble(guard, "min_rate_per_second", 16.0, 0.1, 100000.0, changed);
    normalizedDouble(guard, "activation_rate_per_second", 8.0, 0.0, 100000.0, changed);
    normalizedDouble(guard, "burst_capacity", 32.0, 1.0, 100000.0, changed);
    normalizedBoolean(guard, "log_adjustments", true, changed);

    return changed;
}

void FallingBlockGuard::configure(const nlohmann::json &root)
{
    const auto &guard = root.at("falling_block_guard");

    enabled_ = guard.value("enabled", true);
    control_interval_ticks_ = std::clamp(guard.value("control_interval_ticks", 20), 1, 1200);

    pressure_mspt_threshold_ =
        std::clamp(guard.value("pressure_mspt_threshold", 50.0), 1.0, 10000.0);
    recovery_mspt_threshold_ =
        std::clamp(guard.value("recovery_mspt_threshold", 45.0), 0.0, pressure_mspt_threshold_);
    if (recovery_mspt_threshold_ >= pressure_mspt_threshold_) {
        recovery_mspt_threshold_ = std::max(0.0, pressure_mspt_threshold_ - 5.0);
    }

    backoff_factor_ = std::clamp(guard.value("backoff_factor", 0.5), 0.05, 0.95);
    recovery_factor_ = std::clamp(guard.value("recovery_factor", 2.0), 1.01, 10.0);
    recovery_stable_intervals_ =
        std::clamp(guard.value("recovery_stable_intervals", 5), 1, 600);

    min_rate_per_second_ =
        std::clamp(guard.value("min_rate_per_second", 16.0), 0.1, 100000.0);
    activation_rate_per_second_ =
        std::clamp(guard.value("activation_rate_per_second", 8.0), 0.0, 100000.0);
    burst_capacity_ =
        std::clamp(guard.value("burst_capacity", 32.0), 1.0, 100000.0);

    log_adjustments_ = guard.value("log_adjustments", true);

    states_.clear();
}

void FallingBlockGuard::start()
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

    reschedule();

    if (enabled_) {
        plugin_.getLogger().info(
            "Falling block guard enabled: pressure=" + std::to_string(pressure_mspt_threshold_)
            + "ms, recovery=" + std::to_string(recovery_mspt_threshold_)
            + "ms, floor=" + std::to_string(min_rate_per_second_)
            + "/s, activation=" + std::to_string(activation_rate_per_second_)
            + "/s, backoff=" + std::to_string(backoff_factor_) + "."
        );
    }
    else {
        plugin_.getLogger().info("Falling block guard is disabled by configuration.");
    }
}

void FallingBlockGuard::reschedule()
{
    if (control_task_) {
        control_task_->cancel();
        control_task_.reset();
    }

    states_.clear();

    if (!started_ || !enabled_) {
        return;
    }

    control_task_ = plugin_.getServer().getScheduler().runTaskTimer(
        plugin_,
        [this]() { controlTick(); },
        static_cast<std::uint64_t>(control_interval_ticks_),
        static_cast<std::uint64_t>(control_interval_ticks_)
    );
}

void FallingBlockGuard::stop()
{
    if (control_task_) {
        control_task_->cancel();
        control_task_.reset();
    }

    started_ = false;
    enabled_ = false;
    states_.clear();
}

bool FallingBlockGuard::isFallingBlock(const endstone::Actor &actor)
{
    return actor.getType() == "minecraft:falling_block";
}

void FallingBlockGuard::refillTokens(
    DimensionState &state,
    std::chrono::steady_clock::time_point now
)
{
    if (!state.throttled) {
        return;
    }

    if (state.last_refill.time_since_epoch().count() == 0) {
        state.last_refill = now;
        return;
    }

    const double elapsed =
        std::chrono::duration<double>(now - state.last_refill).count();
    if (elapsed <= 0.0) {
        return;
    }

    state.tokens = std::min(
        burst_capacity_,
        state.tokens + elapsed * state.allowed_rate_per_second
    );
    state.last_refill = now;
}

void FallingBlockGuard::onActorSpawn(endstone::ActorSpawnEvent &event)
{
    if (!enabled_) {
        return;
    }

    auto &actor = event.getActor();
    if (!isFallingBlock(actor)) {
        return;
    }

    auto *dimension = &actor.getDimension();
    auto &state = states_[dimension];
    const auto now = Clock::now();

    if (state.last_control.time_since_epoch().count() == 0) {
        state.last_control = now;
        state.last_refill = now;
    }

    ++state.attempted;

    if (!state.throttled) {
        ++state.accepted;
        return;
    }

    refillTokens(state, now);
    if (state.tokens >= 1.0) {
        state.tokens -= 1.0;
        ++state.accepted;
        return;
    }

    ++state.rejected;
    event.cancel();
}

void FallingBlockGuard::setThrottle(
    endstone::Dimension &dimension,
    DimensionState &state,
    double rate,
    double mspt,
    double attempted_rate,
    const char *reason
)
{
    const auto now = Clock::now();
    const double clamped_rate = std::max(min_rate_per_second_, rate);

    if (state.throttled) {
        refillTokens(state, now);
    }
    else {
        state.throttled = true;
        state.tokens = std::min(burst_capacity_, clamped_rate);
    }

    const bool changed =
        std::abs(state.allowed_rate_per_second - clamped_rate) > 0.01;

    state.allowed_rate_per_second = clamped_rate;
    state.last_refill = now;
    state.recovery_streak = 0;

    if (changed) {
        logAdjustment(dimension, state, mspt, attempted_rate, reason);
    }
}

void FallingBlockGuard::clearThrottle(
    endstone::Dimension &dimension,
    DimensionState &state,
    double mspt,
    double attempted_rate
)
{
    if (!state.throttled) {
        return;
    }

    state.throttled = false;
    state.allowed_rate_per_second = 0.0;
    state.tokens = 0.0;
    state.recovery_streak = 0;
    state.last_refill = Clock::now();

    logAdjustment(dimension, state, mspt, attempted_rate, "recovered");
}

void FallingBlockGuard::logAdjustment(
    const endstone::Dimension &dimension,
    const DimensionState &state,
    double mspt,
    double attempted_rate,
    const char *reason
) const
{
    if (!log_adjustments_) {
        return;
    }

    const std::string allowed =
        state.throttled ? std::to_string(state.allowed_rate_per_second) : std::string("unlimited");

    plugin_.getLogger().warning(
        "Falling block throttle adjusted: dimension=" + dimension.getName()
        + " reason=" + reason
        + " mspt=" + std::to_string(mspt)
        + " attempted_rate=" + std::to_string(attempted_rate) + "/s"
        + " allowed_rate=" + allowed + "/s"
        + " floor=" + std::to_string(min_rate_per_second_) + "/s"
        + " rejected_last_interval=" + std::to_string(state.rejected)
    );
}

void FallingBlockGuard::controlTick()
{
    if (!enabled_ || states_.empty()) {
        return;
    }

    const auto now = Clock::now();
    const std::optional<double> mspt = mspt_query_ ? mspt_query_() : std::nullopt;

    for (auto &[dimension, state] : states_) {
        if (dimension == nullptr) {
            continue;
        }

        if (state.last_control.time_since_epoch().count() == 0) {
            state.last_control = now;
        }

        double elapsed = std::chrono::duration<double>(now - state.last_control).count();
        if (elapsed <= 0.0) {
            elapsed = static_cast<double>(control_interval_ticks_) / 20.0;
        }

        const double attempted_rate = static_cast<double>(state.attempted) / elapsed;
        const double accepted_rate = static_cast<double>(state.accepted) / elapsed;
        state.last_attempt_rate = attempted_rate;
        state.last_accepted_rate = accepted_rate;
        state.last_control = now;

        if (mspt.has_value()) {
            if (*mspt >= pressure_mspt_threshold_) {
                if (state.throttled || attempted_rate >= activation_rate_per_second_) {
                    const double next_rate = state.throttled
                                                 ? state.allowed_rate_per_second * backoff_factor_
                                                 : attempted_rate * backoff_factor_;
                    setThrottle(
                        *dimension,
                        state,
                        next_rate,
                        *mspt,
                        attempted_rate,
                        state.throttled ? "pressure_backoff" : "pressure_enter"
                    );
                }
                else {
                    state.recovery_streak = 0;
                }
            }
            else if (state.throttled && *mspt <= recovery_mspt_threshold_) {
                ++state.recovery_streak;
                if (state.recovery_streak >= recovery_stable_intervals_) {
                    const double next_rate =
                        state.allowed_rate_per_second * recovery_factor_;

                    // attempted_rate counts rejected spawns too, so it represents the
                    // machine's unconstrained demand. If the next step can satisfy that
                    // demand, leave throttled mode entirely.
                    if (next_rate >= std::max(min_rate_per_second_, attempted_rate)) {
                        clearThrottle(*dimension, state, *mspt, attempted_rate);
                    }
                    else {
                        setThrottle(
                            *dimension,
                            state,
                            next_rate,
                            *mspt,
                            attempted_rate,
                            "recovery_step"
                        );
                    }
                }
            }
            else if (state.throttled) {
                state.recovery_streak = 0;
            }
        }
        else if (state.throttled) {
            // Do not make a feedback decision without a fresh MSPT sample. Holding
            // the existing limit avoids oscillating open/closed when PAPI is briefly
            // unavailable, while startup remains unrestricted until a valid sample exists.
            state.recovery_streak = 0;
        }

        state.attempted = 0;
        state.accepted = 0;
        state.rejected = 0;
    }
}
