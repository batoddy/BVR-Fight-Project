#include "control/maneuvers/commit.hpp"

#include <cmath>

FlightCommand Commit::update(const Aircraft& own)
{
    FlightCommand command = own.command;
    command.set_heading_deg(own.picture.bearing_deg)   // irtifa aynı
           .set_throttle(THROTTLE)
           .set_g_limit(G_LIMIT, own.profile.max_g);
    return command;
}

ManeuverStatus Commit::status(const Aircraft& own) const
{
    if (std::abs(own.picture.ata_deg) <= ANGLE_TOLERANCE_DEG)
    {
        return ManeuverStatus::Satisfied;
    }
    return ManeuverStatus::Running;
}
