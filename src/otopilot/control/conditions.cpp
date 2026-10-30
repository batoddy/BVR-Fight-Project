#include "control/conditions.hpp"

#include <cmath>

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

bool condition::enemy_cold(const Aircraft& own)
{
    return std::abs(own.picture.aspect_deg) > 90.0;
}

bool condition::approaching_mar(const Aircraft& own, double margin_nm)
{
    return own.picture.range_3d_nm <= own.picture.mar_nm + margin_nm;
}

bool condition::range_within(const Aircraft& own, double range_nm)
{
    return own.picture.range_3d_nm <= range_nm;
}

bool condition::last_shot_before_mar(const Aircraft& own, double margin_nm)
{
    if (!own.weapons.enemy_in_wez)
    {
        return false;
    }
    if (!own.weapons.f_pole_nm.has_value())
    {
        return false;   // füze yetişemiyor
    }

    const double f_pole_nm = *own.weapons.f_pole_nm;
    const double latest_f_pole_nm = own.picture.mar_nm + margin_nm;
    return f_pole_nm <= latest_f_pole_nm;
}

bool condition::threat_inbound(const Aircraft& own)
{
    return own.picture.threat.has_value();
}

bool condition::threat_within_s(const Aircraft& own, double limit_s)
{
    if (!own.picture.threat.has_value())
    {
        return false;   // gelen füze yok
    }

    const std::optional<double>& time_to_go_s = own.picture.threat->time_to_go_s;
    if (!time_to_go_s.has_value())
    {
        return false;   // füze yaklaşmıyor
    }
    return *time_to_go_s < limit_s;
}

bool condition::own_missile_pitbull(const Aircraft& own)
{
    const Missile* missile = own.weapons.active_missile();
    return missile != nullptr && missile->target.is_pitbull;
}
