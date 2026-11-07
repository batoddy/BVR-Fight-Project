#include "calculations/missile_geometry.hpp"

#include <algorithm>
#include <cmath>

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

// Pitbull'a kalan süre: catch_time_left_s ile aynı uyum, yol pitbull mesafesine kadar.
//   kapatılacak yol = füze–hedef mesafesi − pitbull_range_nm
//   t = accel_travel_time_s(kapatılacak yol, kapanma, yavaşlama)
std::optional<double> missile_geometry::pitbull_time_left_s(const Missile& missile)
{
    const double gap_nm = missile.target.range_nm - missile.profile.pitbull_range_nm;
    if (missile.target.is_pitbull || gap_nm <= 0.0)
    {
        return 0.0;   // zaten pitbull
    }

    return accel_travel_time_s(gap_nm, missile.target.closure_kt, missile.profile.slowing_accel_kt_s);
}

// Güdümlü: füze pitbull ya da atan uçağın burnu hedefe gimbal'dan dar açıyla bakıyor (|ATA| < gimbal).
bool missile_geometry::is_guided(const Missile& missile, const Aircraft& launcher, const Aircraft& target)
{
    if (missile.target.is_pitbull)
    {
        return true;
    }
    const double launcher_ata_deg = geometry::ata_deg(launcher.kinematics, target.kinematics);
    return std::abs(launcher_ata_deg) < launcher.profile.radar_gimbal_deg;
}

// Kaçış payı: şimdi dönüp run_kt ile kaçsam füze ölene kadar aramda kalan mesafe.
// Füze yavaşlaması sabit (profil), benim hızım sabit; füze Mach 1'in altına düşünce ölür.
//   dönüş sonu füze hızı       v_m2  = v_m + a·t_d
//   dönüş sonu kapanma         c2    = v_m2 − run
//   dönüşte kapanan yol        d_t   = (c_now + c2) / 2 · t_d
//   füzenin ölüm hızı          v_son = max(run, Mach 1)          Mach 1 = füze TAS / füze Mach
//   ölene kadar süre           t_son = (v_m2 − v_son) / |a|      v_m2 ≤ v_son ise 0
//   ölene kadar kapanan yol    x     = c2·t_son + a·t_son² / 2
//   pay = füze–ben mesafesi − patlama yarıçapı − d_t − x
double missile_geometry::escape_margin_nm(const Missile& missile, double turn_s, double run_kt)
{
    const double slowing_accel_kt_s = missile.profile.slowing_accel_kt_s;   // negatif
    const double missile_speed_kt = missile.kinematics.tas_kt;
    const double sound_speed_kt = missile.kinematics.tas_kt / missile.kinematics.mach;

    const double speed_after_turn_kt = missile_speed_kt + slowing_accel_kt_s * turn_s;
    const double closure_after_turn_kt = speed_after_turn_kt - run_kt;
    const double average_turn_closure_kt = (missile.target.closure_kt + closure_after_turn_kt) / 2.0;
    const double turn_closed_nm = travel_distance_nm(average_turn_closure_kt, turn_s);

    const double end_speed_kt = std::max(run_kt, sound_speed_kt);   // füze kapanma sıfırlanınca ya da Mach 1'in altında biter
    double time_to_end_s = 0.0;
    if (speed_after_turn_kt > end_speed_kt)
    {
        time_to_end_s = (speed_after_turn_kt - end_speed_kt) / -slowing_accel_kt_s;
    }
    const double run_closed_nm = accel_travel_distance_nm(closure_after_turn_kt, slowing_accel_kt_s, time_to_end_s);

    const double blast_radius_nm = feet_to_nmile(missile.profile.blast_radius_ft);
    return missile.target.range_nm - blast_radius_nm - turn_closed_nm - run_closed_nm;
}
