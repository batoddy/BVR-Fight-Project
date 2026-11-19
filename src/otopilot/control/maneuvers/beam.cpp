#include "control/maneuvers/beam.hpp"

#include <cmath>

#include "common/angles.hpp"

void Beam::enter(const Aircraft& own)
{
    side_ = ManeuverSide::Right;      // düşman solumda: sağa dönerim
    if (own.picture.ata_deg > 0.0)
    {
        side_ = ManeuverSide::Left;   // düşman sağımda: sola dönerim
    }
}

FlightCommand Beam::update(const Aircraft& own)
{
    double beam_heading_deg = own.picture.bearing_deg + 90.0;   // Right: düşman 9 hizamda (ATA −90)
    if (side_ == ManeuverSide::Left)
    {
        beam_heading_deg = own.picture.bearing_deg - 90.0;        // Left: düşman 3 hizamda (ATA +90)
    }

    FlightCommand command = own.command;   // irtifa aynı
    command.set_heading_deg(beam_heading_deg)
           .set_throttle(THROTTLE)
           .set_g_limit(G_LIMIT, own.profile.max_g);
    return command;
}

ManeuverStatus Beam::status(const Aircraft& own) const
{
    double target_ata_deg = -90.0;
    if (side_ == ManeuverSide::Left)
    {
        target_ata_deg = 90.0;
    }
    if (std::abs(wrap_180(target_ata_deg - own.picture.ata_deg)) <= ANGLE_TOLERANCE_DEG)
    {
        return ManeuverStatus::Satisfied;
    }
    return ManeuverStatus::Running;
}
