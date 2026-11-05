#include "calculations/missile_geometry.hpp"

#include <cmath>

#include "calculations/geometry.hpp"
#include "common/units.hpp"

double missile_geometry::range_nm(const Missile& missile, const Aircraft& target)
{
    return geometry::range_3d_nm(missile.kinematics, target.kinematics);
}

double missile_geometry::closure_kt(const Missile& missile, const Aircraft& target)
{
    return geometry::closure_kt(missile.kinematics, target.kinematics);
}

bool missile_geometry::is_pitbull(const Missile& missile, double range_nm)
{
    const double pitbull_range_nm = missile.profile.pitbull_range_nm;
    const bool has_own_seeker = pitbull_range_nm > 0.0;   // yalnız Fox3; Fox1/Fox2'de 0
    const bool close_enough = range_nm < pitbull_range_nm;
    return has_own_seeker && close_enough;
}

// Füzenin sabit ivmeli uçuş uyumu (boost bitişinden sayılır).
//   hız v(t) = v0 + a·t         (kt, a: kt/s, negatif)
//   yol x(t) = v0·t + a·t²/2    (kt·s; NM için 3600'e bölünür)

double missile_geometry::velocity_at_time(const MissileProfile& missile_profile, double t_s)
{
    return missile_profile.start_speed_kt + missile_profile.slowing_accel_kt_s * t_s;
}

double missile_geometry::distance_at_time(const MissileProfile& missile_profile, double t_s)
{
    const double distance_kt_s = missile_profile.start_speed_kt * t_s + 0.5 * missile_profile.slowing_accel_kt_s * t_s * t_s;
    return distance_kt_s / SECONDS_PER_HOUR;
}

std::optional<double> missile_geometry::time_at_distance(const MissileProfile& missile_profile, double distance_nm)
{
    const double a = missile_profile.slowing_accel_kt_s;
    const double v0 = missile_profile.start_speed_kt;
    const double distance_kt_s = distance_nm * SECONDS_PER_HOUR;

    if (distance_kt_s < 0.0)
    {
        return std::nullopt;   // negatif yol: füze zaten geçmiş
    }

    if (a == 0.0)
    {
        return distance_kt_s / v0;
    }

    const double discriminant = v0 * v0 + 2.0 * a * distance_kt_s;
    if (discriminant < 0.0)
    {
        return std::nullopt;   // füze hız sıfıra inmeden bu yola erişemez
    }

    return (-v0 + std::sqrt(discriminant)) / a;
}
