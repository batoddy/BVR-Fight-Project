#include "maneuver.hpp"
#include "angles.hpp"

FlightCommand Hold::compute(const Aircraft& fighter, const FlightCommand& current)
{
    (void)fighter;     // hold ölçüme bakmaz
    return current;    // aynı yön, irtifa ve gaz
}

FlightCommand Crank::compute(const Aircraft& fighter, const FlightCommand& current)
{
    FlightCommand command = current;   // irtifa ve gaz aynı kalsın; yalnız yönü değiştireceğiz

    // TODO: crank hesabı. Elindekiler:
    //   fighter.enemy.bearing_deg_180   karşı uçağın yönü (±180)
    //   fighter.kinematics.heading_deg_180
    //   command.heading_deg_180 = wrap_180(...);
    (void)fighter;

    return command;
}
