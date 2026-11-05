#include "control/otopilot_hostile.hpp"

#include "control/conditions.hpp"
#include "model/missile_profiles.hpp"

void OtopilotHostile::step(Aircraft& own, const Aircraft& enemy)
{
    switch (state_)
    {
    case HostileState::Commit:
        maneuvers_.fly(own, maneuvers_.hold);
        if (condition::inside_enemy_mar(own, enemy))
        {
            go_to(HostileState::Fire);   // düşman kendi MAR'ına girdi: şimdi at
        }
        break;

    case HostileState::Fire:
        maneuvers_.fly(own, maneuvers_.fire);
        own.weapons.fire(AIM120C);
        go_to(HostileState::Out);
        break;

    case HostileState::Out:
        maneuvers_.fly(own, maneuvers_.out);
        if (maneuvers_.out.status(own) == ManeuverStatus::Done)
        {
            own.command.set_throttle(DEFAULT_THROTTLE);
            go_to(HostileState::HoldAfterOut);
        }
        break;

    case HostileState::HoldAfterOut:
        maneuvers_.fly(own, maneuvers_.hold);
        break;
    }
}

void OtopilotHostile::go_to(HostileState next)
{
    state_ = next;
}
