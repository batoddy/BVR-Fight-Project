#include "control/maneuvers/out.hpp"

#include <algorithm>
#include <cmath>

void Out::enter(const Aircraft& own)
{
    const double entry_ft = own.kinematics.altitude_ft;
    target_altitude_ft_ = std::min(entry_ft, std::max(entry_ft - DESCEND_FT, FLOOR_FT));   // girişten alçal; FLOOR_FT altındaysam tırmanmam
    turn_done_ = false;
}

FlightCommand Out::update(const Aircraft& own)
{
    const double away_from_enemy_deg = own.picture.bearing_deg - 180.0;   // düşman tam arkamda
    turn_done_ = turn_done_ || status(own) == ManeuverStatus::Done;   // bir kez bitince kalır: ATA titreşse de gaz değişmez
    double throttle = TURN_THROTTLE;
    if (turn_done_)
    {
        throttle = RUN_THROTTLE;
    }

    FlightCommand command = own.command;
    command.set_heading_deg(away_from_enemy_deg)
           .set_altitude_ft(target_altitude_ft_)
           .set_throttle(throttle)
           .set_g_limit(own.profile.out_g, own.profile.max_g);   // NEZ/MAR hesabıyla aynı g
    return command;
}

ManeuverStatus Out::status(const Aircraft& own) const
{
    const double enemy_angle_off_nose_deg = std::abs(own.picture.ata_deg);
    if (enemy_angle_off_nose_deg > DONE_ATA_DEG)   // düşman arkamda kaldı
    {
        return ManeuverStatus::Done;
    }
    return ManeuverStatus::Running;
}
