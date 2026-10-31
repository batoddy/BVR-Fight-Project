#include "control/otopilot_hostile.hpp"

#include "control/conditions.hpp"
#include "model/missile_profiles.hpp"

void OtopilotHostile::step(Aircraft& own, const Aircraft& enemy)
{
    switch (state_)
    {
    case HostileState::Commit:
        fly(own, maneuvers_.in);
        if (condition::inside_enemy_mar(own, enemy))
        {
            go_to(HostileState::Fire, maneuvers_.fire, own);   // düşman kendi MAR'ına girdi: şimdi at
        }
        break;

    case HostileState::Fire:
        fly(own, maneuvers_.fire);
        own.weapons.fire(AIM120C);
        go_to(HostileState::Out, maneuvers_.out, own);
        break;

    case HostileState::Out:
        fly(own, maneuvers_.out);
        if (maneuvers_.out.status(own) == ManeuverStatus::Done)
        {
            own.command.set_throttle(DEFAULT_THROTTLE);
            go_to(HostileState::HoldAfterOut, maneuvers_.hold, own);
        }
        break;

    case HostileState::HoldAfterOut:
        fly(own, maneuvers_.hold);
        break;
    }
}

void OtopilotHostile::go_to(HostileState next, Maneuver& maneuver, const Aircraft& own)
{
    state_ = next;
    maneuver.enter(own);
}
