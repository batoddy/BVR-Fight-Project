#include "control/maneuvers/hold.hpp"

#include <cmath>

#include "common/angles.hpp"

void Hold::enter(const Aircraft& own)
{
    target_heading_deg_ = own.kinematics.heading_deg;
    target_altitude_ft_ = own.kinematics.altitude_ft;
    has_target_ = true;
}

FlightCommand Hold::update(const Aircraft& own)
{
    if (!has_target_)
    {
        enter(own);
    }
    FlightCommand command = own.command;
    command.set_heading_deg(target_heading_deg_)
           .set_altitude_ft(target_altitude_ft_)
           .set_throttle(THROTTLE)
           .set_g_limit(G_LIMIT, own.profile.max_g);
    return command;
}

ManeuverStatus Hold::status(const Aircraft& own) const
{
    const double heading_error_deg = wrap_180(target_heading_deg_ - own.kinematics.heading_deg);
    const double altitude_error_ft = target_altitude_ft_ - own.kinematics.altitude_ft;
    if (has_target_ && std::abs(heading_error_deg) <= ANGLE_TOLERANCE_DEG
        && std::abs(altitude_error_ft) <= ALTITUDE_TOLERANCE_FT)
    {
        return ManeuverStatus::Satisfied;
    }
    return ManeuverStatus::Running;
}
