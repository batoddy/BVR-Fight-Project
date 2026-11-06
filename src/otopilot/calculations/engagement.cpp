#include "calculations/engagement.hpp"

#include <algorithm>
#include <cmath>

#include "calculations/geometry.hpp"
#include "calculations/missile_geometry.hpp"
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
//   dış sınır = Rmax, azami kinematik menzil: füze hedefe enerjisi bitmeden erişir (isabet şart değil)
//     Rmax = füzenin kendi gittiği yol + füze uçarken düşmanın bana doğru aldığı yol
//     füzenin kendi yolu = (a + b·ln h) · (1 + k·(benim hızım − referans hız))    h: benim irtifam, km
//     uçuş süresi        = c + d·ln h
//     düşmanın aldığı yol = düşmanın bana doğru hızı × uçuş süresi
//   Düşman bana geliyorsa menzil uzar, benden kaçıyorsa (enemy_closure_kt < 0) kısalır.
// Katsayılar BVRGym füzesine karşı yapılan erişim taramasından: calculations/wez_formulu.md
Wez engagement::wez_nm(const Aircraft& own, const Aircraft& enemy)
{
    const MissileProfile& missile = own.profile.main_weapon;
    const RmaxFit& fit = missile.r_max;
    const double enemy_closure_kt = geometry::other_closure_kt(own.kinematics, enemy.kinematics);

    const double altitude_km = feet_to_meter(own.kinematics.altitude_ft) / 1000.0;
    const double ln_altitude = std::log(altitude_km);
    const double launch_speed_factor = 1.0 + fit.k * (own.kinematics.tas_kt - fit.launch_ref_kt);

    const double missile_path_nm = (fit.a + fit.b * ln_altitude) * launch_speed_factor;
    const double flight_time_s = fit.c + fit.d * ln_altitude;
    const double enemy_approach_nm = travel_distance_nm(enemy_closure_kt, flight_time_s);

    Wez result;
    result.min_nm = missile.min_range_nm;
    result.max_nm = missile_path_nm + enemy_approach_nm;
    return result;
}

bool engagement::in_wez(const Aircraft& own, const Aircraft& enemy)
{
    const Wez own_wez = engagement::wez_nm(own, enemy);
    const double range_nm = geometry::range_3d_nm(own.kinematics, enemy.kinematics);

    const bool beyond_min = range_nm > own_wez.min_nm;
    const bool within_max = range_nm < own_wez.max_nm;
    return beyond_min && within_max;
}

std::optional<double> engagement::a_pole_nm(const Aircraft& own, const Aircraft& enemy)
{
    const double range_nm = own.picture.range_3d_nm - own.profile.main_weapon.pitbull_range_nm;
    const std::optional<double> flight_time_s = missile_geometry::time_at_distance(own.profile.main_weapon, range_nm);
    if (!flight_time_s)
    {
        return std::nullopt;   // füze o mesafeye erişemez
    }
    return (range_nm - travel_distance_nm(own.picture.closure_kt, *flight_time_s) - 1);
}

std::optional<double> engagement::f_pole_nm(const Aircraft& own, const Aircraft& enemy)
{
    const double range_nm = own.picture.range_3d_nm;
    const std::optional<double> flight_time_s = missile_geometry::time_at_distance(own.profile.main_weapon, range_nm);
    if (!flight_time_s)
    {
        return std::nullopt;   // füze o mesafeye erişemez
    }
    return (range_nm - travel_distance_nm(own.picture.closure_kt, *flight_time_s) - 1);
}


std::optional<double> engagement::tr_nm(const Aircraft& own, const Aircraft& enemy)
{
    const double range_nm = own.picture.range_3d_nm - own.profile.main_weapon.pitbull_range_nm;
    const std::optional<double> flight_time_s = missile_geometry::time_at_distance(own.profile.main_weapon, range_nm);
    if (!flight_time_s)
    {
        return std::nullopt;   // füze o mesafeye erişemez
    }
    return (range_nm - travel_distance_nm(own.picture.closure_kt, *flight_time_s) - 1);
}



// Out planı: şimdi out yapsam ne olur.
//   Dönüş:       düşmanın bearing'i + 180°'ye (düşmanı tam arkama alacak yöne), kısa taraftan, profildeki out g'siyle.
//   Kaçış hızı:  şimdilik dönüşte hız korunur kabul edilir (kaçış hızı = şu anki TAS).
//   Yaklaşma:    dönüş bitince düşmana olan yatay mesafe ne kadar azaldı.
//                Dönüş boyunca ben yer değiştiririm; düşman yerinde kabul edilir.
// Açısız sürüm şu anki ATA'yı kullanır. Açılı sürüm: düşman burnuma göre ata_deg'deyken out yapsam ne olur.

OutPlan engagement::out_plan(const Aircraft& own, const Aircraft& enemy)
{
    return out_plan(own, enemy, geometry::ata_deg(own.kinematics, enemy.kinematics));
}

OutPlan engagement::out_plan(const Aircraft& own, const Aircraft& enemy, double ata_deg)
{
    OutPlan out;
    out.escape_kt = escape_speed_kt(own);

    const double bearing_deg = geometry::bearing_deg(own.kinematics, enemy.kinematics);
    const double away_from_enemy_deg = bearing_deg + 180.0;

    // ATA = bearing − heading: yalnız başlık planlanan açıya göre değişir, konum ve hız aynı kalır.
    Kinematics planned = own.kinematics;
    planned.heading_deg = bearing_deg - ata_deg;
    out.turn = turn::to_heading(planned, away_from_enemy_deg, own.profile.out_g, out.escape_kt);

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
//
// Ne anlatır: düşman füzeyi attığı anda ben kuyruğumu dönmüş, y hızıyla düz kaçıyorsam, füze en fazla
// R_cold uzaktan atılırsa bana yetişir. Daha uzaktan atılan füze yolda Mach 1'in altına düşer, kaybolur.
// Füzenin zamana göre nerede olduğunu VERMEZ; yalnız "yetişir / yetişemez" sınırıdır.
//
// Nasıl bulundu: BVRGym'de 36 kaçış senaryosunda (4-10 km, farklı kaçış hızları) her biri için
// "yetişti / yetişemedi" sınırı ikiye bölme ile ölçüldü; ln(R_cold) bu ölçümlere doğrusal uyduruldu.
// Geçerli aralık: 4-10 km, 570-1000 kt.

double engagement::r_cold_nm(const Aircraft& own, const Aircraft& enemy)
{
    // Düşmanın ana silahına ait katsayılar (her füze tipinin kendi uyumu olur).
    const RColdFit& fit = enemy.profile.main_weapon.r_cold;

    // x: irtifa farkı, km. Merkez 10 km; x = 0 iken formül 10 km'deki değeri verir.
    //    Yükseklik arttıkça hava seyrelir, füze daha az yavaşlar, R_cold büyür (b > 0).
    const double altitude_km = feet_to_meter(own.kinematics.altitude_ft) / 1000.0;
    const double x = altitude_km - 10.0;

    // y: kaçış hızı farkı, kt. Merkez 700 kt; y = 0 iken formül 700 kt'deki değeri verir.
    //    Daha hızlı kaçarsam füzenin kapatması gereken yol uzar, R_cold küçülür (c < 0).
    const double y = escape_speed_kt(own) - 700.0;

    // Üs: a  → 10 km ve 700 kt'deki ln(R_cold)       (e^2.646 ≈ 14.1 NM)
    //     b·x → irtifa etkisi
    //     c·y → kaçış hızı etkisi
    //     d·x·y → hız etkisinin irtifayla değişimi (alçakta hız daha az fark ettirir)
    const double exponent = fit.a + fit.b * x + fit.c * y + fit.d * x * y;

    // Güvenlik çarpanı (1.1): formül bazı noktalarda gerçeğinden küçük çıkıyordu; kaçış hesabı güvenli tarafta kalsın.
    return fit.safety * std::exp(exponent);
}



// NEZ (No Escape Zone): füze dönüşe başladığımız an atılırsa, bu mesafenin içinde kaçamayız.
// Füze dönüşümüz boyunca uçar; biz o sürede kaçamayız, dönüş bitince kaçmaya başlarız.
//   NEZ = R_cold + yaklaşma + dönerken kaçamadığımız yol
//     yaklaşma:                   dönüşte düşmana yaklaşmamız (out planı)
//     dönerken kaçamadığımız yol: kaçış hızı × dönüş süresi
double engagement::nez_nm(const Aircraft& own, const Aircraft& enemy)
{
    return nez_nm(own, enemy, geometry::ata_deg(own.kinematics, enemy.kinematics));
}

double engagement::nez_nm(const Aircraft& own, const Aircraft& enemy, double ata_deg)
{
    const OutPlan out = out_plan(own, enemy, ata_deg);

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
    return mar_nm(own, enemy, geometry::ata_deg(own.kinematics, enemy.kinematics));
}

double engagement::mar_nm(const Aircraft& own, const Aircraft& enemy, double ata_deg)
{
    const OutPlan out = out_plan(own, enemy, ata_deg);
    const double enemy_closure_kt = geometry::other_closure_kt(own.kinematics, enemy.kinematics);

    const double fire_at_start_nm = nez_nm(own, enemy, ata_deg);

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

// MRR (Minimum Recommit Range), Cold Ops'ta bakılır: buradan dönüp atarsam MAR'da hâlâ kaçabilir miyim.
// Şu anki burun yönüyle hesaplanan MAR kullanılmaz: cold uçarken dönüş payı ~0 çıkar.
//   Recommit sonrası MAR = recommit atış açısından (recommit_fire_ata_deg) out yapılırsa MAR
//   MRR = recommit sonrası MAR + 150° recommit dönüşünde düşmanın yaklaşması + 0,5 NM
double engagement::mrr_cold_ops_nm(const Aircraft& own, const Aircraft& enemy)
{
    const double planned_mar_nm = mar_nm(own, enemy, own.profile.recommit_fire_ata_deg);

    const double speed_kt = own.kinematics.tas_kt;
    const double radius_m = turn::radius_m(speed_kt, own.profile.out_g);
    const double in_turn_s = turn::time_s(180 - own.profile.recommit_fire_ata_deg, radius_m, speed_kt);
    const double enemy_approach_nm = travel_distance_nm(enemy.kinematics.tas_kt, in_turn_s);

    return planned_mar_nm + enemy_approach_nm + 0.5;
}

// DR (Decision Range): STERN WEZ + düşmanın notch beklemesi boyunca yaklaştığı yol + mesafe payı.
//   STERN WEZ = R_cold; notch beklemesi varsayılan 15 s, ek pay sabit 0,5 NM.
//   Yalnız düşmanın bana doğru hız bileşeni kullanılır; TAS veya iki uçağın toplam kapanması değil.
//   Hız işaretlidir: düşman uzaklaşıyorsa yaklaşma terimi negatiftir.
double engagement::dr_nm(const Aircraft& own, const Aircraft& enemy)
{
    const double stern_wez_nm = engagement::r_cold_nm(own, enemy);
    const double enemy_approach_nm = travel_distance_nm(enemy.kinematics.tas_kt, own.profile.dr_notch_time_s);
    return stern_wez_nm + enemy_approach_nm + 0.5;
}

// DOR = MAR + 180°- radar_gimal_deg - recommit dönüşü boyunca yaklaşması + out payı.
// Recommit dönüş süresi, mevcut hız ve out_g ile hesaplanır; hız ve yaklaşma bileşeni sabit kabul edilir.
double engagement::dor_nm(const Aircraft& own, const Aircraft& enemy)
{
    const double speed_kt = own.kinematics.tas_kt;
    const double radius_m = turn::radius_m(speed_kt, own.profile.out_g);
    const double in_turn_s = turn::time_s(180 - own.profile.recommit_fire_ata_deg, radius_m, speed_kt);
    const double enemy_approach_nm = travel_distance_nm(enemy.kinematics.tas_kt, in_turn_s);

    return engagement::mar_nm(own, enemy) + enemy_approach_nm + 0.5;
}

bool engagement::in_dor(const Aircraft& own, const Aircraft& enemy)
{
    const double range_nm = geometry::range_3d_nm(own.kinematics, enemy.kinematics);
    return range_nm <= dor_nm(own, enemy);
}

std::optional<double> engagement::closest_stay_away_nm(const std::vector<ThreatInfo>& threats)
{
    std::optional<double> closest_nm;
    for (const ThreatInfo& threat : threats)
    {
        if (threat.stay_away_nm.has_value() && (!closest_nm.has_value() || *threat.stay_away_nm > *closest_nm))
        {
            closest_nm = threat.stay_away_nm;
        }
    }
    return closest_nm;
}
