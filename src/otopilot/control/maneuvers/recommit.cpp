#include "control/maneuvers/recommit.hpp"

#include <cmath>

namespace
{
    ManeuverSide recommit_side_for(const Aircraft& own)   // kısa taraf: düşman hangi tarafımdaysa o tarafa dön
    {
        if (own.picture.ata_deg >= 0.0)
        {
            return ManeuverSide::Right;
        }
        return ManeuverSide::Left;
    }
}

void Recommit::enter(const Aircraft& own)
{
    side_ = recommit_side_for(own);
    has_reached_ = false;
}

// ATA = bearing − heading. Right: düşman burnun sağında kalır (ATA +), Left: solunda (ATA −).
FlightCommand Recommit::update(const Aircraft& own)
{
    const double fire_ata_deg = own.profile.recommit_fire_ata_deg;

    if (status(own) == ManeuverStatus::Done)
    {
        has_reached_ = true;   // bir kez vardıktan sonra 20°'ye geri dönülmez
    }
    double target_ata_deg = fire_ata_deg - TURN_INSIDE_DEG;   // 30 − 10 = 20: dönüş 30°'ye kadar sert kalsın
    if (has_reached_)
    {
        target_ata_deg = fire_ata_deg;                         // 30°'de dur
    }

    FlightCommand command = own.command;   // irtifa aynı
    command.set_throttle(THROTTLE)
           .set_g_limit(own.profile.out_g, own.profile.max_g);   // sert dönüş: out ile aynı g
    if (side_ == ManeuverSide::Right)
    {
        command.set_heading_deg(own.picture.bearing_deg - target_ata_deg);
    }
    else
    {
        command.set_heading_deg(own.picture.bearing_deg + target_ata_deg);
    }
    return command;
}

ManeuverStatus Recommit::status(const Aircraft& own) const
{
    const double enemy_angle_off_nose_deg = std::abs(own.picture.ata_deg);
    if (enemy_angle_off_nose_deg <= own.profile.recommit_fire_ata_deg + DONE_TOLERANCE_DEG)   // atış açısına indi
    {
        return ManeuverStatus::Done;
    }
    return ManeuverStatus::Running;
}
