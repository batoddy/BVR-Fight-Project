#include "control/maneuvers/notch.hpp"

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

FlightCommand Notch::update(const Aircraft& own)
{
    const double bearing_deg = escape_bearing_deg(own);
    double notch_heading_deg = bearing_deg + 90.0;   // tehdit solumda kalsın
    if (escape_ata_deg(own) > 0.0)
    {
        notch_heading_deg = bearing_deg - 90.0;      // tehdit sağımda kalsın
    }

    FlightCommand command = own.command;   // irtifa aynı
    command.set_heading_deg(notch_heading_deg)
           .set_throttle(THROTTLE)
           .set_g_limit(G_LIMIT, own.profile.max_g);
    return command;
}

ManeuverStatus Notch::status(const Aircraft& own) const
{
    if (std::abs(std::abs(escape_ata_deg(own)) - 90.0) <= ANGLE_TOLERANCE_DEG)   // tehdit 3 ya da 9 hizamda
    {
        return ManeuverStatus::Satisfied;
    }
    return ManeuverStatus::Running;
}
