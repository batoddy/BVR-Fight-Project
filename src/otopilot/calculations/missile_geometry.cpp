#include "calculations/missile_geometry.hpp"

#include "calculations/geometry.hpp"
#include "common/formulas.hpp"
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

double missile_geometry::speed_at_time_kt(const MissileProfile& missile_profile, double t_s)
{
    return missile_profile.start_speed_kt + missile_profile.slowing_accel_kt_s * t_s;
}

double missile_geometry::distance_at_time_nm(const MissileProfile& missile_profile, double t_s)
{
    return accel_travel_distance_nm(missile_profile.start_speed_kt, missile_profile.slowing_accel_kt_s, t_s);
}

std::optional<double> missile_geometry::time_at_distance_s(const MissileProfile& missile_profile, double distance_nm)
{
    if (distance_nm < 0.0)
    {
        return std::nullopt;   // negatif yol: füze zaten geçmiş
    }

    return accel_travel_time_s(distance_nm, missile_profile.start_speed_kt, missile_profile.slowing_accel_kt_s);   // hız sıfıra inmeden erişemezse yok
}

// Füzenin hedefe yetişmesine kalan süre. Hedefin hareketi kapanma hızıyla hesaba girer:
//   kapatılacak yol = füze ile hedef arası mesafe − patlama yarıçapı
//   kapanma şu an target.closure_kt; füze uyumdaki gibi yavaşlar, hedef hızını korur kabul edilir
//   t = accel_travel_time_s(kapatılacak yol, kapanma, yavaşlama)
// Kapanma, yol kapanmadan sıfıra inerse füze yetişemez: std::nullopt.
std::optional<double> missile_geometry::catch_time_left_s(const Missile& missile)
{
    return catch_time_left_s(missile, missile.target.closure_kt);
}

std::optional<double> missile_geometry::catch_time_left_s(const Missile& missile, double closure_kt)
{
    const double blast_radius_nm = feet_to_nmile(missile.profile.blast_radius_ft);
    const double gap_nm = missile.target.range_nm - blast_radius_nm;
    if (gap_nm <= 0.0)
    {
        return 0.0;   // hedef zaten patlama yarıçapının içinde
    }

    return accel_travel_time_s(gap_nm, closure_kt, missile.profile.slowing_accel_kt_s);
}
