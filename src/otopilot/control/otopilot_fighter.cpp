#include "control/otopilot_fighter.hpp"

#include "control/conditions.hpp"

// Gazı ve g sınırını manevralar yazar; atış durumları yalnız atış emri verir, manevra uçmaz.
// Atış emri kabul edilmezse (önceki emir henüz uçmadı) aynı durumda kalınır, sonraki step tekrar denenir.
void OtopilotFighter::step(Aircraft& own, const Aircraft& enemy)
{
    (void)enemy;

    const MissileProfile& weapon = own.profile.main_weapon;

    switch (state_)
    {
    case OtopilotState::Commit:
        maneuvers_.fly(own, maneuvers_.commit);
        if (condition::in_mar(own))
        {
            go_to(OtopilotState::FirstEscape);   // atmadan MAR'a girdim: kaç
        }
        else if (condition::enemy_in_my_tr(own))
        {
            go_to(OtopilotState::FirstShot);
        }
        break;

    case OtopilotState::FirstShot:
        if (own.weapons.fire(weapon))
        {
            go_to(OtopilotState::FirstSupport);
        }
        break;

    case OtopilotState::FirstSupport:
        maneuvers_.fly(own, maneuvers_.crank);
        if (condition::in_dor(own))
        {
            go_to(OtopilotState::FirstEscape);
        }
        break;

    case OtopilotState::FirstEscape:
        maneuvers_.fly(own, maneuvers_.out);
        if (maneuvers_.out.status(own) == ManeuverStatus::Done)
        {
            go_to(OtopilotState::Recommit);
        }
        break;

    case OtopilotState::Recommit:
        maneuvers_.fly(own, maneuvers_.recommit);
        if (maneuvers_.recommit.status(own) == ManeuverStatus::Done)
        {
            go_to(OtopilotState::SecondShot);
        }
        break;

    case OtopilotState::SecondShot:
        if (own.weapons.fire(weapon))
        {
            go_to(OtopilotState::SecondSupport);
        }
        break;

    case OtopilotState::SecondSupport:
        maneuvers_.fly(own, maneuvers_.crank);
        if (condition::in_mar(own))
        {
            go_to(OtopilotState::FinalEscape);
        }
        break;

    case OtopilotState::FinalEscape:
        maneuvers_.fly(own, maneuvers_.out);
        if (maneuvers_.out.status(own) == ManeuverStatus::Done)
        {
            go_to(OtopilotState::Departure);
        }
        break;

    case OtopilotState::Departure:
        maneuvers_.fly(own, maneuvers_.hold);
        break;

    default:   // hostile'ın durumları (Fire, Out, HoldAfterOut, Hold) fighter akışında yok
        break;
    }
}

void OtopilotFighter::go_to(OtopilotState next)
{
    state_ = next;
}
