#pragma once

#include <cstdint>

namespace ecleaner::falling_block {

// Spawn cancellation is destructive, so an explicit opt-in is mandatory.
[[nodiscard]] inline bool shouldRejectSpawn(bool reject_mode, bool throttled, double tokens)
{
    return reject_mode && throttled && tokens < 1.0;
}

enum class PressureAction {
    Hold,
    Enter,
    Backoff,
};

// High MSPT alone does not establish that falling-block spawns cause the load.
// Do not reduce an allowance when no blocks are attempted or demand is below it.
[[nodiscard]] inline PressureAction pressureAction(
    bool throttled,
    std::uint64_t attempted,
    std::uint64_t rejected,
    double attempted_rate,
    double allowed_rate,
    double activation_rate
)
{
    if (attempted == 0) {
        return PressureAction::Hold;
    }
    if (!throttled) {
        return attempted_rate >= activation_rate ? PressureAction::Enter : PressureAction::Hold;
    }
    return rejected > 0 || attempted_rate > allowed_rate
               ? PressureAction::Backoff
               : PressureAction::Hold;
}

}  // namespace ecleaner::falling_block
