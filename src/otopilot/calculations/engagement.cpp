#include "calculations/engagement.hpp"

#include <algorithm>
#include <cmath>

#include "calculations/geometry.hpp"
#include "calculations/turn.hpp"
#include "common/units.hpp"

namespace
{
    // kt, kaçış hızı: şimdilik dönüşte hız korunur kabul edilir (kaçış hızı = şu anki TAS).
    double escape_speed_kt(const Aircraft& own)
    {
        return own.kinematics.tas_kt;
    }
}

// WEZ: füzenin etkin menzil aralığı.
//   iç sınır = füzenin en kısa menzili
//   dış sınır = füzenin en uzun menzili + füze uçarken düşmanın bana doğru aldığı yol
//     Düşman bana geliyorsa menzil uzar, benden kaçıyorsa (enemy_closure_kt < 0) kısalır.
Wez engagement::wez(const Aircraft& own, const Aircraft& enemy)
{
    const MissileProfile& missile = own.profile.main_weapon;
    const double enemy_closure_kt = geometry::other_closure_kt(own.kinematics, enemy.kinematics);

    Wez result;
    result.min_nm = missile.min_range_nm;
    result.max_nm = missile.max_range_nm;

    if (missile.speed_kt > 0.0)
    {
        const double flight_time_s = travel_time_s(missile.max_range_nm, missile.speed_kt);
        const double enemy_approach_nm = travel_distance_nm(enemy_closure_kt, flight_time_s);
        result.max_nm = missile.max_range_nm + enemy_approach_nm;
    }

    return result;
}

bool engagement::in_wez(const Aircraft& own, const Aircraft& enemy)
{
    const Wez own_wez = wez(own, enemy);
    const double range_nm = geometry::range_3d_nm(own.kinematics, enemy.kinematics);

    const bool beyond_min = range_nm > own_wez.min_nm;
    const bool within_max = range_nm < own_wez.max_nm;
    return beyond_min && within_max;
}

// F-pole: füze hedefe vardığında iki uçak arasında kalan mesafe.
//   füzenin hedefe yaklaşma hızı = füze hızı + hedefin bana doğru hızı
//   füzenin uçuş süresi          = mesafe / füzenin yaklaşma hızı
//   F-pole                       = mesafe − (iki uçağın bu sürede kapattığı yol)
std::optional<double> engagement::f_pole_nm(const Aircraft& own, const Aircraft& enemy)
{
    const MissileProfile& missile = own.profile.main_weapon;
    const double range_nm = geometry::range_3d_nm(own.kinematics, enemy.kinematics);
    const double enemy_closure_kt = geometry::other_closure_kt(own.kinematics, enemy.kinematics);
    const double closure_kt = geometry::closure_kt(own.kinematics, enemy.kinematics);

    const double missile_closure_kt = missile.speed_kt + enemy_closure_kt;
    if (missile_closure_kt <= 0.0)
    {
        return std::nullopt;   // hedef füzeden hızlı kaçıyor: füze yetişemez
    }

    const double flight_time_s = travel_time_s(range_nm, missile_closure_kt);
    const double closed_nm = travel_distance_nm(closure_kt, flight_time_s);
    return range_nm - closed_nm;
}

// Out planı: şimdi out yapsam ne olur.
//   Dönüş:       düşmanın bearing'i + 180°'ye (düşmanı tam arkama alacak yöne), kısa taraftan, profildeki out g'siyle.
//   Kaçış hızı:  şimdilik dönüşte hız korunur kabul edilir (kaçış hızı = şu anki TAS).
//   Yaklaşma:    dönüş bitince düşmana olan yatay mesafe ne kadar azaldı.
//                Dönüş boyunca ben yer değiştiririm; düşman yerinde kabul edilir.
OutPlan engagement::out_plan(const Aircraft& own, const Aircraft& enemy)
{
    OutPlan out;
    out.escape_kt = escape_speed_kt(own);

    const double away_from_enemy_deg = geometry::bearing_deg(own.kinematics, enemy.kinematics) + 180.0;
    out.turn = turn::to_heading(own.kinematics, away_from_enemy_deg, own.profile.out_g, out.escape_kt);

    // Düşmanın bana göre konumu (kuzey, doğu): şimdi ve dönüş bittiğinde.
    const Ned enemy_now_nm = geometry::los_nm(own.kinematics, enemy.kinematics);
    const double enemy_after_north_nm = enemy_now_nm.north - out.turn.displacement_nm.north;
    const double enemy_after_east_nm = enemy_now_nm.east - out.turn.displacement_nm.east;

    // Yatay mesafe, Pisagor: √(kuzey² + doğu²)
    const double range_now_nm = std::sqrt(enemy_now_nm.north * enemy_now_nm.north
                                          + enemy_now_nm.east * enemy_now_nm.east);
    const double range_after_nm = std::sqrt(enemy_after_north_nm * enemy_after_north_nm
                                            + enemy_after_east_nm * enemy_after_east_nm);

    out.approach_nm = range_now_nm - range_after_nm;   // + ise dönüşte düşmana yaklaştım
    return out;
}

// R_cold: baştan kaçıyor olsaydık düşman füzesinin bize yetişebileceği en büyük mesafe.
//   R_cold = güvenlik × e^(a + b·x + c·y + d·x·y)
//     x = benim irtifam − 10 km
//     y = benim kaçış hızım − 700 kt
// Katsayılar (a, b, c, d, güvenlik) düşmanın ana silahının uyumundan: calculations/r_cold_formulu.md
double engagement::r_cold_nm(const Aircraft& own, const Aircraft& enemy)
{
    const RColdFit& fit = enemy.profile.main_weapon.r_cold;

    const double altitude_km = feet_to_meter(own.kinematics.altitude_ft) / 1000.0;
    const double x = altitude_km - 10.0;                 // km
    const double y = escape_speed_kt(own) - 700.0;       // kt

    const double exponent = fit.a + fit.b * x + fit.c * y + fit.d * x * y;
    return fit.safety * std::exp(exponent);
}

// NEZ (No Escape Zone): füze dönüşe başladığımız an atılırsa, bu mesafenin içinde kaçamayız.
// Füze dönüşümüz boyunca uçar; biz o sürede kaçamayız, dönüş bitince kaçmaya başlarız.
//   NEZ = R_cold + yaklaşma + dönerken kaçamadığımız yol
//     yaklaşma:                   dönüşte düşmana yaklaşmamız (out planı)
//     dönerken kaçamadığımız yol: kaçış hızı × dönüş süresi
double engagement::nez_nm(const Aircraft& own, const Aircraft& enemy)
{
    const OutPlan out = out_plan(own, enemy);

    const double lost_while_turning_nm = travel_distance_nm(out.escape_kt, out.turn.time_s);
    return r_cold_nm(own, enemy) + out.approach_nm + lost_while_turning_nm;
}

// MAR (Minimum Abort Range): dönüşün herhangi bir anında atılan füzeden kaçabileceğimiz en kısa mesafe.
// Düşman dönüş boyunca herhangi bir anda atabilir. İki uç durum:
//   1) dönüş başında atar: füze dönüş boyunca uçmuş, biz kaçamamışız   → NEZ
//   2) dönüş sonunda atar: düşman dönüş boyunca bize yaklaşmış         → R_cold + yaklaşma + düşmanın yaklaştığı yol
// Arası doğrusal değişir; en kötüsü iki uçtan büyük olanıdır.
//   MAR = en kötü durum + patlama yarıçapı + out payı
double engagement::mar_nm(const Aircraft& own, const Aircraft& enemy)
{
    const OutPlan out = out_plan(own, enemy);
    const double enemy_closure_kt = geometry::other_closure_kt(own.kinematics, enemy.kinematics);

    const double fire_at_start_nm = nez_nm(own, enemy);

    const double enemy_approach_nm = travel_distance_nm(enemy_closure_kt, out.turn.time_s);
    const double fire_at_end_nm = r_cold_nm(own, enemy) + out.approach_nm + enemy_approach_nm;

    const double worst_case_nm = std::max(fire_at_start_nm, fire_at_end_nm);
    const double blast_radius_nm = feet_to_nmile(enemy.profile.main_weapon.blast_radius_ft);
    return worst_case_nm + blast_radius_nm + own.profile.out_offset_nm;
}

bool engagement::in_mar(const Aircraft& own, const Aircraft& enemy)
{
    const double range_nm = geometry::range_3d_nm(own.kinematics, enemy.kinematics);
    return range_nm <= mar_nm(own, enemy);
}

double engagement::dr_nm()
{
    return 0;
}

// DOR (Desired Out Range): out yaptıktan sonra geri dönüp tekrar angaje olabileceğim (recommit) mesafe.
// Recommit dönüşü (180°) boyunca düşman bana yaklaşmaya devam eder; dönüş bitince hâlâ MAR'ın dışında olmalıyım.
//   DOR = MAR + düşmanın recommit dönüşü boyunca yaklaştığı yol
//   recommit dönüşü: 180°, out g'siyle, şu anki hızla
// 180° dönüşte yer değişimi yana doğrudur (2r); düşmana yaklaşma yoktur.
double engagement::dor_nm(const Aircraft& own, const Aircraft& enemy)
{
    const double speed_kt = own.kinematics.tas_kt;
    const double radius_m = turn::radius_m(speed_kt, own.profile.out_g);
    const double recommit_turn_s = turn::time_s(180.0, radius_m, speed_kt);

    const double enemy_closure_kt = geometry::other_closure_kt(own.kinematics, enemy.kinematics);
    const double enemy_approach_nm = travel_distance_nm(enemy_closure_kt, recommit_turn_s);

    return mar_nm(own, enemy) + enemy_approach_nm;
}
