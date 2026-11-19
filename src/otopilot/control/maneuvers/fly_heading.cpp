#include "control/maneuvers/fly_heading.hpp"

#include <cmath>
#include <stdexcept>

#include "common/angles.hpp"

void FlyHeading::set(double heading_deg)
{
    if (!std::isfinite(heading_deg))
    {
        throw std::invalid_argument("FlyHeading yonu sonlu olmalidir");
    }
    heading_deg_ = wrap_180(heading_deg);
}

FlightCommand FlyHeading::update(const Aircraft& own)
{
    FlightCommand command = own.command;
    command.set_heading_deg(heading_deg_)
           .set_throttle(THROTTLE)
           .set_g_limit(G_LIMIT, own.profile.max_g);

    return command;
}

ManeuverStatus FlyHeading::status(const Aircraft& own) const
{
    if (std::abs(wrap_180(heading_deg_ - own.kinematics.heading_deg)) <= ANGLE_TOLERANCE_DEG)
    {
        return ManeuverStatus::Satisfied;
    }
    return ManeuverStatus::Running;
}
