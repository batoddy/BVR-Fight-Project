#include "control/otopilot_hostile.hpp"

#include "control/conditions.hpp"
#include "model/missile_profiles.hpp"

// Gazı ve g sınırını manevralar yazar; atış durumu yalnız atış emri verir, manevra uçmaz.
void OtopilotHostile::step(Aircraft& own, const Aircraft& enemy)
{
    (void)enemy;

    switch (state_)
    {
    case OtopilotState::Commit:
        maneuvers_.fly(own, maneuvers_.commit);
        if (condition::inside_enemy_mar(own))
        {
            go_to(OtopilotState::Fire);   // düşman kendi MAR'ına girdi: şimdi at
        }
        break;

    case OtopilotState::Fire:
        if (own.weapons.fire(own.profile.main_weapon))
        {
            go_to(OtopilotState::Out);
        }
        break;

    case OtopilotState::Out:
        maneuvers_.fly(own, maneuvers_.out);
        if (maneuvers_.out.status(own) == ManeuverStatus::Done)
        {
            go_to(OtopilotState::HoldAfterOut);
        }
        break;

    case OtopilotState::HoldAfterOut:
        maneuvers_.fly(own, maneuvers_.hold);
        break;

    default:   // fighter'ın durumları hostile akışında yok
        break;
    }
}

void OtopilotHostile::go_to(OtopilotState next)
{
    state_ = next;
}
