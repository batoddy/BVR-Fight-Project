#include "control/maneuvers/maneuvers.hpp"

void Maneuvers::fly(Aircraft& own, Maneuver& maneuver)
{
    if (active_ != &maneuver)   // yeni manevra: girişe bağlı hedefleri kilitle
    {
        maneuver.enter(own);
        active_ = &maneuver;
    }
    own.phase = maneuver.phase();
    own.command = maneuver.update(own);
}
