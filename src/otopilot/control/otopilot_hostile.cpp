/*
#include "control/otopilot_hostile.hpp"

#include <cmath>

FlightCommand OtopilotHostile::step(
    Aircraft& hostile,
    const World& world,
    const FlightCommand& current)
{
    // Python'un son uyguladığı komutla başla.
    command_ = current;

    switch (state_)
    {
    case 0:
        hostile.phase = Phase::Hold;
        command_ = maneuvers_.hold.compute(hostile, command_);

        if (hostile.enemy.range_2d_nm <= 25.0)
        {
            state_ = 1;
        }
        break;

    case 1:
        hostile.phase = Phase::Fire;
        hostile.fire_missile(FOX3);
        state_ = 2;
        break;

    case 2:
        hostile.phase = Phase::Crank;
        command_ = maneuvers_.crank_right.compute(hostile, command_);

        if(hostile.enemy.is_in_mar)
        {
            maneuvers_.out.begin(hostile);   
            state_ = 4;
        }

        else if(hostile.last_fired()->target.pitbull)
        {
            state_ = 3;
        }

        break;
    
    case 3:
        hostile.phase = Phase::In;
        command_ = maneuvers_.in.compute(hostile, command_);
        break;
    
    default:
        break;
    }

    return command_;
}
*/


#include "control/otopilot_hostile.hpp"

#include <cmath>

FlightCommand OtopilotHostile::step(
    Aircraft& hostile,
    const World& world,
    const FlightCommand& current)
{
    // Mevcut hostile taktiği ek dünya bilgisi kullanmıyor.
    (void)world;
    command_ = current;
    
    
    switch (state_)
    {
    case 0:
        hostile.phase = Phase::Hold;
        command_ = maneuvers_.hold.compute(hostile, command_);

        if (world.fighter.enemy.mar_nm >= hostile.enemy.range_3d_nm)
        {
            state_ = 1;
        }
        break;

    case 1:
        hostile.phase = Phase::Fire;
        hostile.fire_missile(FOX3);
        state_ = 2;
        break;

    case 2:
        hostile.phase = Phase::Hold;
        command_ = maneuvers_.hold.compute(hostile, command_);
        break;

    default:
        break;
    }

    return command_;
}
