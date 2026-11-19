#include "control/maneuvers/crank.hpp"

#include <cmath>

#include "common/angles.hpp"

namespace
{
    ManeuverSide crank_side_for(const Aircraft& own)
    {
        if (own.picture.ta_deg >= 0.0)
        {
            return ManeuverSide::Left;    // düşmanın burnunun sağındayım: sola dönerim
        }
        return ManeuverSide::Right;       // düşmanın burnunun solundayım: sağa dönerim
    }
}

void Crank::enter(const Aircraft& own)
{
    side_ = crank_side_for(own);
}

// ATA = bearing − heading. Right: düşman burnun solunda kalır (ATA −), Left: sağında (ATA +).
FlightCommand Crank::update(const Aircraft& own)
{
    const double limit_deg = own.profile.radar_gimbal_deg - OFFSET_DEG;
    FlightCommand command = own.command;   // irtifa aynı
    command.set_throttle(THROTTLE)
           .set_g_limit(G_LIMIT, own.profile.max_g);
    if (side_ == ManeuverSide::Right)
    {
        command.set_heading_deg(own.picture.bearing_deg + limit_deg);
    }
    else
    {
        command.set_heading_deg(own.picture.bearing_deg - limit_deg);
    }
    return command;
}

bool Crank::in_radar_sight(const Aircraft& own) const
{
    return std::abs(own.picture.ata_deg) < own.profile.radar_gimbal_deg;
}

ManeuverStatus Crank::status(const Aircraft& own) const
{
    double target_ata_deg = own.profile.radar_gimbal_deg - OFFSET_DEG;
    if (side_ == ManeuverSide::Right)
    {
        target_ata_deg = -target_ata_deg;
    }
    const double angle_error_deg = wrap_180(target_ata_deg - own.picture.ata_deg);
    if (in_radar_sight(own) && std::abs(angle_error_deg) <= ANGLE_TOLERANCE_DEG)
    {
        return ManeuverStatus::Satisfied;
    }
    return ManeuverStatus::Running;
}
