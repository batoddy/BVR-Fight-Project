#include "control/maneuvers/drag.hpp"

#include <cmath>

#include "calculations/geometry.hpp"
#include "model/missile.hpp"

namespace
{
    // Kaçılan yön: bana gelen füze varsa füze, yoksa düşman uçak.
    double escape_bearing_deg(const Aircraft& own)
    {
        const Missile* threat = own.picture.threat;
        return threat ? geometry::bearing_deg(own.kinematics, threat->kinematics) : own.picture.bearing_deg;
    }

    double escape_ata_deg(const Aircraft& own)
    {
        const Missile* threat = own.picture.threat;
        return threat ? geometry::ata_deg(own.kinematics, threat->kinematics) : own.picture.ata_deg;
    }
}

FlightCommand Drag::update(const Aircraft& own)
{
    FlightCommand command = own.command;   // irtifa aynı
    command.set_heading_deg(escape_bearing_deg(own) - 180.0)   // tehdit tam arkamda
           .set_throttle(THROTTLE)
           .set_g_limit(own.profile.out_g, own.profile.max_g);
    return command;
}

ManeuverStatus Drag::status(const Aircraft& own) const
{
    if (std::abs(escape_ata_deg(own)) >= 180.0 - ANGLE_TOLERANCE_DEG)   // tehdit 6 hizamda
    {
        return ManeuverStatus::Satisfied;
    }
    return ManeuverStatus::Running;
}
