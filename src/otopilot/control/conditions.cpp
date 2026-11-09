#include "control/conditions.hpp"

#include <cmath>
#include <optional>

#include "model/missile.hpp"

namespace
{
    constexpr double HOT_MAX_DEG = 30.0;     // derece, bu açıya kadar hot
    constexpr double FLANK_MAX_DEG = 70.0;   // derece, bu açıya kadar flank
    constexpr double BEAM_MAX_DEG = 110.0;   // derece, bu açıya kadar beam; ötesi cold
    constexpr double HARMLESS_RECOMMIT_MARGIN_NM = 1.0;   // NM, zararsız füze: füzeye dönsem bile bu kadar pay kalmalı

    Aspect aspect_of(double angle_deg)   // açıyı (±180) sınıfa çevirir; yön değil büyüklük
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

bool condition::in_dor(const Aircraft& own)
{
    return own.picture.is_in_dor;
}

bool condition::enemy_in_my_wez(const Aircraft& own)
{
    return own.weapons.enemy_in_wez;
}

bool condition::enemy_in_my_tr(const Aircraft& own)
{
    return own.weapons.enemy_in_tr;
}

bool condition::enemy_in_my_tr_plus(const Aircraft& own, double extra_nm)
{
    return own.weapons.tr_nm && own.picture.range_3d_nm <= *own.weapons.tr_nm + extra_nm;
}

bool condition::enemy_in_my_msr(const Aircraft& own)
{
    return own.weapons.enemy_in_msr;
}

bool condition::enemy_in_my_r_cold(const Aircraft& own)
{
    return own.picture.range_3d_nm <= own.weapons.r_cold_nm;
}

bool condition::beyond_mrr(const Aircraft& own)
{
    return own.picture.range_3d_nm >= own.picture.mrr_nm;
}

Aspect condition::enemy_aspect(const Aircraft& own)
{
    return aspect_of(own.picture.ta_deg);
}

Aspect condition::my_aspect(const Aircraft& own)
{
    return aspect_of(own.picture.ata_deg);
}

bool condition::enemy_cold(const Aircraft& own)
{
    return enemy_aspect(own) == Aspect::Cold;
}

bool condition::enemy_hot(const Aircraft& own)
{
    const Aspect aspect = enemy_aspect(own);
    return aspect == Aspect::Hot || aspect == Aspect::Flank;
}

bool condition::has_live_missile(const Aircraft& own)
{
    return own.weapons.launch_pending() || own.weapons.active_missile() != nullptr;
}

bool condition::out_of_missiles(const Aircraft& own)
{
    return own.weapons.missiles_left() == 0 && !has_live_missile(own);
}

bool condition::has_guided_threat(const Aircraft& own)
{
    return own.picture.threat != nullptr;
}

// Zararsız füze: benden yavaş ve recommit yapsam bile füze ölene kadar arada pay kalıyor.
bool condition::threat_harmless(const Aircraft& own)
{
    if (!has_guided_threat(own))
    {
        return false;
    }
    const bool slower_than_me = own.picture.threat->kinematics.tas_kt < own.kinematics.tas_kt;
    const std::optional<double> recommit_margin_nm = own.picture.threat_recommit_margin_nm;
    return slower_than_me && recommit_margin_nm && *recommit_margin_nm >= HARMLESS_RECOMMIT_MARGIN_NM;
}

bool condition::threat_alive(const Aircraft& own)
{
    return has_guided_threat(own) && own.picture.threat->target.catch_time_left_s.has_value() && !threat_harmless(own);
}

bool condition::supporting(const Aircraft& own)
{
    const Missile* missile = own.weapons.active_missile();
    return own.weapons.launch_pending() || (missile != nullptr && !missile->target.is_pitbull);
}

bool condition::is_spiked(const Aircraft& own)
{
    return own.picture.is_spiked;
}
