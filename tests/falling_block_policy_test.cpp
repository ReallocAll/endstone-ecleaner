#include "falling_block_policy.h"

#include <cstdlib>

using ecleaner::falling_block::PressureAction;
using ecleaner::falling_block::pressureAction;

int main()
{
    auto require = [](bool ok) { if (!ok) std::abort(); };
    require(pressureAction(false, 0, 0, 0.0, 0.0, 0.0) == PressureAction::Hold);
    require(pressureAction(true, 0, 0, 0.0, 64.0, 16.0) == PressureAction::Hold);
    require(pressureAction(false, 10, 0, 15.0, 0.0, 16.0) == PressureAction::Hold);
    require(pressureAction(false, 10, 0, 16.0, 0.0, 16.0) == PressureAction::Enter);
    require(pressureAction(true, 10, 0, 30.0, 286.0, 16.0) == PressureAction::Hold);
    require(pressureAction(true, 10, 0, 16.0, 16.0, 16.0) == PressureAction::Hold);
    require(pressureAction(true, 10, 0, 100.0, 64.0, 16.0) == PressureAction::Backoff);
    require(pressureAction(true, 10, 2, 30.0, 64.0, 16.0) == PressureAction::Backoff);
}
