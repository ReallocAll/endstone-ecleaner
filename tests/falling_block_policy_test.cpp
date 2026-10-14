#include "falling_block_policy.h"

#include <cassert>

using ecleaner::falling_block::PressureAction;
using ecleaner::falling_block::pressureAction;

int main()
{
    assert(pressureAction(false, 0, 0, 0.0, 0.0, 0.0) == PressureAction::Hold);
    assert(pressureAction(true, 0, 0, 0.0, 64.0, 16.0) == PressureAction::Hold);
    assert(pressureAction(false, 10, 0, 15.0, 0.0, 16.0) == PressureAction::Hold);
    assert(pressureAction(false, 10, 0, 16.0, 0.0, 16.0) == PressureAction::Enter);
    assert(pressureAction(true, 10, 0, 30.0, 286.0, 16.0) == PressureAction::Hold);
    assert(pressureAction(true, 10, 0, 16.0, 16.0, 16.0) == PressureAction::Hold);
    assert(pressureAction(true, 10, 0, 100.0, 64.0, 16.0) == PressureAction::Backoff);
    assert(pressureAction(true, 10, 2, 30.0, 64.0, 16.0) == PressureAction::Backoff);
}
