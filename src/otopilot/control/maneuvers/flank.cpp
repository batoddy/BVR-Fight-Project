#include "control/maneuvers/flank.hpp"

#include <cmath>
#include <stdexcept>

#include "calculations/geometry.hpp"
#include "common/angles.hpp"

void Flank::set(double target_ta_deg, double tolerance_deg)
{
    if (!std::isfinite(target_ta_deg) || !std::isfinite(tolerance_deg)
        || tolerance_deg < 0.0 || tolerance_deg > 180.0)
    {
        throw std::invalid_argument("Flank acisi sonlu, tolerans 0..180 derece olmalidir");
    }
    target_ta_deg_ = wrap_180(target_ta_deg);
    tolerance_deg_ = tolerance_deg;
}

void Flank::enter(const Aircraft& own)
{
    range_nm_ = own.picture.range_2d_nm;
}

FlightCommand Flank::update(const Aircraft& own)
{
    FlightCommand command = own.command;
    command.set_heading_deg(geometry::flank_heading_deg(own.kinematics, own.picture, target_ta_deg_, range_nm_, RESPONSE_S))
           .set_throttle(THROTTLE)
           .set_g_limit(G_LIMIT, own.profile.max_g);

    return command;
}

ManeuverStatus Flank::status(const Aircraft& own) const
{
    const double angle_error_deg = wrap_180(target_ta_deg_ - own.picture.ta_deg);
    if (std::abs(angle_error_deg) <= tolerance_deg_)
    {
        return ManeuverStatus::Satisfied;
    }
    return ManeuverStatus::Running;
}
