#include "control/maneuvers/offset.hpp"

#include <cmath>
#include <stdexcept>

#include "common/angles.hpp"

void Offset::set(double offset_deg, ManeuverSide side)
{
    if (!std::isfinite(offset_deg) || offset_deg < 0.0 || offset_deg > 180.0)
    {
        throw std::invalid_argument("Offset acisi sonlu ve 0..180 derece olmalidir");
    }
    offset_deg_ = offset_deg;
    side_ = side;
}

FlightCommand Offset::update(const Aircraft& own)
{
    double target_heading_deg = own.picture.bearing_deg + offset_deg_;
    if (side_ == ManeuverSide::Left)
    {
        target_heading_deg = own.picture.bearing_deg - offset_deg_;
    }
    FlightCommand command = own.command;
    command.set_heading_deg(target_heading_deg)
           .set_throttle(THROTTLE)
           .set_g_limit(G_LIMIT, own.profile.max_g);

    return command;
}

ManeuverStatus Offset::status(const Aircraft& own) const
{
    double target_ata_deg = -offset_deg_;
    if (side_ == ManeuverSide::Left)
    {
        target_ata_deg = offset_deg_;
    }
    if (std::abs(wrap_180(target_ata_deg - own.picture.ata_deg)) <= ANGLE_TOLERANCE_DEG)
    {
        return ManeuverStatus::Satisfied;
    }
    return ManeuverStatus::Running;
}
