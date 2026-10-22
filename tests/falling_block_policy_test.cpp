#include "falling_block_policy.h"

#include <cstdlib>
#include <initializer_list>

using ecleaner::falling_block::PressureAction;
using ecleaner::falling_block::pressureAction;

int main()
{
    auto require = [](bool ok) { if (!ok) std::abort(); };
    using ecleaner::falling_block::shouldRejectSpawn;
    for (bool throttled : {false, true}) {
        for (double tokens : {0.0, 0.5, 1.0, 32.0}) {
            require(!shouldRejectSpawn(false, throttled, tokens));
            require(shouldRejectSpawn(true, throttled, tokens) == (throttled && tokens < 1.0));
        }
    }
    require(pressureAction(false, 0, 0, 0.0, 0.0, 0.0) == PressureAction::Hold);
    require(pressureAction(true, 0, 0, 0.0, 64.0, 16.0) == PressureAction::Hold);
    require(pressureAction(false, 10, 0, 15.0, 0.0, 16.0) == PressureAction::Hold);
    require(pressureAction(false, 10, 0, 16.0, 0.0, 16.0) == PressureAction::Enter);
    require(pressureAction(true, 10, 0, 30.0, 286.0, 16.0) == PressureAction::Hold);
    require(pressureAction(true, 10, 0, 16.0, 16.0, 16.0) == PressureAction::Hold);
    require(pressureAction(true, 10, 0, 100.0, 64.0, 16.0) == PressureAction::Backoff);
    require(pressureAction(true, 10, 2, 30.0, 64.0, 16.0) == PressureAction::Backoff);
}
