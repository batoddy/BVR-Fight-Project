#include "control/otopilot_fighter.hpp"

#include "control/conditions.hpp"
#include "model/missile_profiles.hpp"

void OtopilotFighter::step(Aircraft& own, const Aircraft& enemy)
{
    (void)enemy;

    switch (state_)
    {
    case FighterState::Commit:
        fly(own, maneuvers_.in);
        if (condition::in_mar(own))
        {
            go_to(FighterState::Out, maneuvers_.out, own);   // atmadan MAR'a girdim: kaç
        }
        else if (condition::enemy_in_my_wez(own))
        {
            go_to(FighterState::Fire, maneuvers_.fire, own);
        }
        break;

    case FighterState::Fire:
        fly(own, maneuvers_.fire);
        own.weapons.fire(AIM120C);
        go_to(FighterState::Crank, maneuvers_.crank, own);
        break;

    case FighterState::Crank:
        fly(own, maneuvers_.crank);
        if (condition::in_mar(own))
        {
            go_to(FighterState::Out, maneuvers_.out, own);
        }
        break;

    case FighterState::Out:
        fly(own, maneuvers_.out);
        if (maneuvers_.out.status(own) == ManeuverStatus::Done)
        {
            own.command.set_throttle(DEFAULT_THROTTLE);
            go_to(FighterState::HoldAfterOut, maneuvers_.hold, own);
        }
        break;

    case FighterState::HoldAfterOut:
        fly(own, maneuvers_.hold);
        break;
    }
}

void OtopilotFighter::go_to(FighterState next, Maneuver& maneuver, const Aircraft& own)
{
    state_ = next;
    maneuver.enter(own);
}
