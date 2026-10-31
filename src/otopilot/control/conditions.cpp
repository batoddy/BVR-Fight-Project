#include "control/conditions.hpp"

#include <cmath>

namespace
{
    constexpr double HOT_MAX_DEG = 30.0;     // derece, bu açıya kadar hot
    constexpr double FLANK_MAX_DEG = 70.0;   // derece, bu açıya kadar flank
    constexpr double BEAM_MAX_DEG = 110.0;   // derece, bu açıya kadar beam; ötesi cold

    // Açıyı (±180) sınıfa çevirir; yön değil büyüklük kullanılır.
    Aspect aspect_of(double angle_deg)
    {
        const double size_deg = std::abs(angle_deg);

        if (size_deg <= HOT_MAX_DEG)
        {
            return Aspect::Hot;
        }
        if (size_deg <= FLANK_MAX_DEG)
        {
            return Aspect::Flank;
        }
        if (size_deg <= BEAM_MAX_DEG)
        {
            return Aspect::Beam;
        }
        return Aspect::Cold;
    }
}

bool condition::in_mar(const Aircraft& own)
{
    return own.picture.is_in_mar;
}

bool condition::inside_enemy_mar(const Aircraft& own, const Aircraft& enemy)
{
    return enemy.picture.mar_nm >= own.picture.range_3d_nm;
}

bool condition::enemy_in_my_wez(const Aircraft& own)
{
    return own.weapons.enemy_in_wez;
}

bool condition::enemy_in_my_nez(const Aircraft& own)
{
    return own.weapons.enemy_in_nez;
}

Aspect condition::enemy_aspect(const Aircraft& own)
{
    return aspect_of(own.picture.aspect_deg);
}

Aspect condition::my_aspect(const Aircraft& own)
{
    return aspect_of(own.picture.ata_deg);
}

bool condition::threat_inbound(const Aircraft& own)
{
    return own.picture.threat.has_value();
}

// atış sırasına göre slot değil !
bool condition::own_missile_pitbull(const Aircraft& own, int fire_order)
{
    for (const Missile& missile : own.weapons.missiles)
    {
        if (missile.fire_order == fire_order)
        {
            return missile.target.is_pitbull;
        }
    }
    return false;   // o sırada atılmış füze yok
}
