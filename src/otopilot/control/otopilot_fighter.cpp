#include "control/otopilot_fighter.hpp"

#include <cmath>

FlightCommand OtopilotFighter::step(
    Aircraft& fighter,
    const World& world,
    const FlightCommand& current)
{
    // Python'un son uyguladığı komutla başla.
    command_ = current;

    switch (state_)
    {
    case 0:
        fighter.phase = Phase::Hold;
        command_ = maneuvers_.hold.compute(fighter, command_);

        if (fighter.enemy.is_in_mar)
        {
            state_ = 1;
            maneuvers_.out.begin(fighter);
        }
        break;

    case 1:
        fighter.phase = Phase::Out;
        command_ = maneuvers_.out.compute(fighter, command_);
        if (std::fabs(fighter.enemy.ata_deg) > 177)
        {
            state_ = 2;
            command_.throttle = 0.45;
        }
        break;

    case 2:
        fighter.phase = Phase::Hold;
        command_ = maneuvers_.hold.compute(fighter, command_);
        break;

    default:
        break;
    }

    return command_;
}


// #include "control/otopilot_fighter.hpp"

// #include <cmath>

// FlightCommand OtopilotFighter::step(
//     Aircraft& fighter,
//     const World& world,
//     const FlightCommand& current)
// {
//     // Python'un son uyguladığı komutla başla.
//     command_ = current;

//     switch (state_)
//     {
//     case 0:
//         fighter.phase = Phase::Hold;
//         command_ = maneuvers_.hold.compute(fighter, command_);

//         if (fighter.enemy.range_2d_nm <= 25.0)
//         {
//             state_ = 1;
//         }
//         break;

//     case 1:
//         fighter.phase = Phase::Fire;
//         fighter.fire_missile(FOX3);
//         state_ = 2;
//         break;

//     case 2:
//         fighter.phase = Phase::Crank;
//         command_ = maneuvers_.crank_right.compute(fighter, command_);

//         if(fighter.enemy.is_in_mar)
//         {
//             maneuvers_.out.begin(fighter);   
//             state_ = 4;
//         }

//         else if(fighter.last_fired()->target.pitbull)
//         {
//             state_ = 3;
//         }

//         break;
    
//     case 3:
//         fighter.phase = Phase::Notch;
//         command_ = maneuvers_.notch.compute(fighter, command_);

//         if(fighter.enemy.is_in_mar)
//         {
//             maneuvers_.out.begin(fighter);   
//             state_ = 4;
//         }

//         break;
    
//     case 4:
//         fighter.phase = Phase::Out;
//         command_ = maneuvers_.out.compute(fighter, command_);
//         break;
    
//     default:
//         break;
//     }

//     return command_;
// }