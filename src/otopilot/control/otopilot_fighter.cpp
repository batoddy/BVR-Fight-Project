#include "control/otopilot_fighter.hpp"

#include "control/conditions.hpp"
#include "model/missile_profiles.hpp"

// void OtopilotFighter::step(Aircraft& own, const Aircraft& enemy)
// {
//     (void)enemy;

//     switch (state_)
//     {
//     case FighterState::Commit:
//         maneuvers_.fly(own, maneuvers_.hold);
//         if (condition::in_mar(own))
//         {
//             go_to(FighterState::FirstEscape);   // atmadan MAR'a girdim: kaç
//         }
//         else if (own.picture.range_3d_nm < FIRST_SHOT_RANGE_NM)
//         {
//             go_to(FighterState::FirstShot);
//         }
//         break;

//     case FighterState::FirstShot:
//         maneuvers_.fly(own, maneuvers_.fire);
//         own.weapons.fire(AIM120C);
//         go_to(FighterState::FirstSupport);
//         break;

//     case FighterState::FirstSupport:
//         maneuvers_.fly(own, maneuvers_.crank);
//         if (condition::in_dor(own))
//         {
//             go_to(FighterState::FirstEscape);
//         }
//         break;

//     case FighterState::FirstEscape:
//         maneuvers_.fly(own, maneuvers_.out);
//         if (maneuvers_.out.status(own) == ManeuverStatus::Done)
//         {
//             own.command.set_throttle(DEFAULT_THROTTLE);
//             go_to(FighterState::Recommit);
//         }
//         break;

//     case FighterState::Recommit:
//     own.command.set_throttle(0.99);
//         maneuvers_.fly(own, maneuvers_.recommit);
//         if (maneuvers_.recommit.status(own) == ManeuverStatus::Done)
//         {
//             own.command.set_throttle(DEFAULT_THROTTLE);
//             go_to(FighterState::SecondShot);
//         }
//         break;

//     case FighterState::SecondShot:
//         maneuvers_.fly(own, maneuvers_.fire);
//         own.weapons.fire(AIM120C);
//         go_to(FighterState::SecondSupport);
//         break;

//     case FighterState::SecondSupport:
//         maneuvers_.fly(own, maneuvers_.crank);
//         if (condition::in_mar(own))
//         {
//             go_to(FighterState::FinalEscape);
//         }
//         break;

//     case FighterState::FinalEscape:
//         maneuvers_.fly(own, maneuvers_.out);
//         if (maneuvers_.out.status(own) == ManeuverStatus::Done)
//         {
//             own.command.set_throttle(DEFAULT_THROTTLE);
//             go_to(FighterState::Departure);
//         }
//         break;

//     case FighterState::Departure:
//         maneuvers_.fly(own, maneuvers_.hold);
//         break;
//     }
// }



void OtopilotFighter::step(Aircraft& own, const Aircraft& enemy)
{
    (void)enemy;

    switch (state_)
    {
    // Deneme: 50, 40, 30, 20 NM'de birer atış. Kalan füze sayısı hangi atışta olduğumuzu söyler (4 füze).
    case FighterState::Commit:
        maneuvers_.fly(own, maneuvers_.hold);
        if (own.weapons.missiles_left() == 4 && own.picture.range_3d_nm < 50.0)
        {
            own.weapons.fire(AIM120C);
        }
        else if (own.weapons.missiles_left() == 3 && own.picture.range_3d_nm < 40.0)
        {
            own.weapons.fire(AIM120C);
        }
        else if (own.weapons.missiles_left() == 2 && own.picture.range_3d_nm < 30.0)
        {
            own.weapons.fire(AIM120C);
        }
        else if (own.weapons.missiles_left() == 1 && own.picture.range_3d_nm < 20.0)
        {
            own.weapons.fire(AIM120C);
        }
        break;

    case FighterState::FirstShot:
        maneuvers_.fly(own, maneuvers_.fire);
        own.weapons.fire(AIM120C);
        go_to(FighterState::FirstSupport);
        break;

    case FighterState::Departure:
        maneuvers_.fly(own, maneuvers_.hold);
        break;
    }
}


void OtopilotFighter::go_to(FighterState next)
{
    state_ = next;
}
