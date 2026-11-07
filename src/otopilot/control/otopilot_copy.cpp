#include "control/otopilot.hpp"

#include <string>

void Otopilot::step(Aircraft& own, const Aircraft& enemy)
{
    (void)enemy;

    const MissileProfile& weapon = own.profile.main_weapon;

    switch (state_)
    {
    case OtopilotState::Commit:
        maneuvers_.fly(own, maneuvers_.commit);
        if (own.weapons.enemy_in_msr)   // TR'ye girdim: at
        {
            record(own, "atış", "TR içinde");
            go_to(OtopilotState::Shot);
        }
        break;

    case OtopilotState::Shot:
        if (own.weapons.fire(weapon))   // emir kabul edilmezse sonraki step tekrar
        {
            go_to(OtopilotState::Support);
        }
        break;

    case OtopilotState::Support:
        maneuvers_.fly(own, maneuvers_.crank);
        if (own.picture.is_in_mar)   // DOR'a geldim: out
        {
            record(own, "out", "DOR içinde");
            go_to(OtopilotState::Escape);
        }
        break;

    case OtopilotState::Escape:
        maneuvers_.fly(own, maneuvers_.out);
        break;

    default:
        break;
    }
}

void Otopilot::go_to(OtopilotState next)
{
    state_ = next;
}

void Otopilot::record(Aircraft& own, const char* text, const char* reason) const
{
    own.decision = std::string(text) + " — " + reason + " (" + goal_name(mission_.goal) + " / " + risk_name(mission_.risk) + ")";
}
