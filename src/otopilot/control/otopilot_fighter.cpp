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

        if (fighter.enemy.range_3d_nm < fighter.enemy.mar_nm)
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
/*

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

        if (fighter.enemy.enemy_in_my_wez)
        {
            state_ = 1;
        }
        break;

    case 1:
        fighter.phase = Phase::Fire;
        fighter.fire_missile(FOX3);
        state_ = 2;
        break;

    case 2:
        fighter.phase = Phase::Crank;
        command_ = maneuvers_.crank_right.compute(fighter, command_);

        if (fighter.last_fired()->target.pitbull == true || world.hostile.last_fired()->target.range_nm < 6.479481641468683 || fighter.last_fired()->in_flight() == false)
        {
            state_ = 3;
        }
        break;

    case 3:
        fighter.phase = Phase::Notch;
        command_.throttle = 0.99;
        command_ = maneuvers_.notch.compute(fighter, command_);

        if (!world.hostile.has_active_missile_against(fighter) || world.hostile.last_fired()->target.closure_kt < 97.19222462203024)
        {
            state_ = 4;
        }
        break;

    case 4:
        fighter.phase = Phase::In;
        command_ = maneuvers_.in.compute(fighter, command_);

        if (std::abs(fighter.enemy.ata_deg) < 45 &&
            fighter.enemy.enemy_in_my_wez)
        {
            state_ = 5;
        }
        break;

    case 5:
        fighter.phase = Phase::Fire;
        fighter.fire_missile(FOX3);
        state_ = 6;
        break;

    case 6:
        fighter.phase = Phase::In;
        command_ = maneuvers_.in.compute(fighter, command_);

        if (std::abs(fighter.enemy.ata_deg) < 175)
        {
            state_ = 7;
        }
        break;

    case 7:
        fighter.phase = Phase::Hold;
        command_ = maneuvers_.hold.compute(fighter, command_);
        break;

    default:
        break;
    }

    return command_;
}

*/