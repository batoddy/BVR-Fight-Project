#include "control/maneuver.hpp"
#include <algorithm>   // std::clamp
#include "common/util_functions.hpp"

FlightCommand Hold::compute(const Aircraft& fighter, const FlightCommand& current)
{
    FlightCommand command;     // hold ölçüme bakmaz
    command.altitude_ft = fighter.kinematics.altitude_ft;
    command.heading_deg_180 = fighter.kinematics.heading_deg_180;
    command.throttle = current.throttle;
    
    return command;    // aynı yön, irtifa ve gaz
}

void Out::begin(const Aircraft& fighter)
{
    const double entry_ft = fighter.kinematics.altitude_ft;
    const double wanted_ft = entry_ft - descend_ft_;

    double lowest_ft = floor_ft_;
    if (entry_ft < floor_ft_)
    {
        lowest_ft = entry_ft;
    }

    target_altitude_ft_ = std::max(wanted_ft, lowest_ft);
}

FlightCommand Out::compute(const Aircraft& fighter, const FlightCommand& current)
{
    FlightCommand command = current;

    command.heading_deg_180 = wrap_180(fighter.enemy.bearing_deg - 180);
    command.altitude_ft = target_altitude_ft_;   // begin()'de hesaplanan sabit hedef
    command.throttle = 1.00;                     // tam art yakıcı
    command.g_limit = fighter.profile.out_g;     // NEZ/MAR ile aynı g

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
