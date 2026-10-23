#include "control/maneuver.hpp"
#include <algorithm>   // std::clamp
#include "common/util_functions.hpp"

FlightCommand Hold::compute(const Aircraft& fighter, const FlightCommand& current)
{
    (void)fighter;     // hold ölçüme bakmaz
    return current;    // aynı yön, irtifa ve gaz
}

FlightCommand Out::compute(const Aircraft& fighter, const FlightCommand& current)
{
    FlightCommand command = current;   // irtifa ve gaz aynı kalacak; yalnızca yön değişir

    command.heading_deg_180 = wrap_180(fighter.enemy.bearing_deg - 180);
    command.throttle = 0.99;

    return command;
}

FlightCommand In::compute(const Aircraft& fighter, const FlightCommand& current)
{
    FlightCommand command = current;   // irtifa ve gaz aynı kalacak; yalnızca yön değişir

    command.heading_deg_180 = fighter.enemy.bearing_deg;

    return command;
}


FlightCommand Crank::compute(const Aircraft& fighter, const FlightCommand& current)
{
    FlightCommand command = current;   // irtifa ve gaz aynı kalacak; yalnızca yön değişir

    // Hedef ATA: Left'te düşman sağda (+), Right'ta solda (-)
    double limit_deg = fighter.profile.radar_gimbal_deg - offset_deg_;
    double target_ata_deg = (side_ == CrankSide::Left) ? limit_deg : -limit_deg;

    double error_deg = wrap_180(fighter.enemy.ata_deg - target_ata_deg);

    // P: hatanın kp katı kadar dön. ±180'i aşarsa wrap_180 ters çevirir; sınırla.
    double correction_deg = std::clamp(kp_ * error_deg, -max_correction_deg_, max_correction_deg_);

    command.heading_deg_180 = wrap_180(fighter.kinematics.heading_deg_180 + correction_deg);
    return command;
}

FlightCommand Notch::compute(const Aircraft& fighter, const FlightCommand& current)
{
    FlightCommand command = current;   // irtifa ve gaz aynı kalacak; yalnızca yön değişir

    if(fighter.enemy.bearing_deg > 0)
    {
        command.heading_deg_180 = fighter.enemy.bearing_deg - 90;
    }

    else
    {
        command.heading_deg_180 = fighter.enemy.bearing_deg + 90;
    }

    return command;
}
