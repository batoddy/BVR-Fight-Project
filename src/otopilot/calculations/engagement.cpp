#include "calculations/engagement.hpp"

Wez Engagement::wez(const Aircraft& own)
{
    Wez result;
    if (own.missiles.empty())
    {
        return result;   // füzesi yok: {0, 0}
    }

    const MissileProfile& missile = own.missiles[0].profile;
    result.min_nm = missile.min_range_nm;
    result.max_nm = missile.max_range_nm;

    // Füze uçarken düşman bana doğru gelirse menzil uzar, kaçarsa kısalır.
    if (missile.speed_kt > 0.0)
    {
        double flight_time_s = SECONDS_PER_HOUR * missile.max_range_nm / missile.speed_kt;
        result.max_nm += own.enemy.enemy_closing_kt * flight_time_s / SECONDS_PER_HOUR;
    }

    return result;
}

bool Engagement::in_wez(double range_nm, const Wez& wez)
{
    return range_nm > wez.min_nm && range_nm < wez.max_nm;
}

double Engagement::f_pole_nm(const Aircraft& own, const Aircraft& enemy)
{
    (void)enemy;

    if (own.missiles.empty())
        return 0.0;

    const double missile_speed_kt = own.missiles[0].profile.speed_kt;
    const double range_nm = own.enemy.range_3d_nm;

    // Füzenin hızı + hedefin bize doğru hız bileşeni.
    const double missile_closure_kt =
        missile_speed_kt + own.enemy.enemy_closing_kt;

    if (missile_closure_kt <= 0.0)
        return 0.0;

    // Füzenin hedefe ulaşacağı yaklaşık süre.
    const double hit_time_s = SECONDS_PER_HOUR * range_nm / missile_closure_kt;

    // Başlangıç mesafesi - iki uçağın bu sürede kapattığı mesafe.
    return range_nm - own.enemy.rate_of_closure_kt * hit_time_s / SECONDS_PER_HOUR;
}

// r_cold_nm = s* - V_kaçış · τ*
//   s*, τ*: füzenin hızı V_kaçış'a düştüğünde uçtuğu yol ve geçen süre (glide_table)
// Tablo eşit adımlı: satır = (Mach - ilk Mach) / adım; iki satır arası doğrusal.
double Engagement::r_cold_nm(const Aircraft& own, const Aircraft& enemy)
{
    (void)enemy;

    // Kaçan hedefe atış taramasından (4-10 km, 560-870 kt) üstel uyum.
    const double x = feet_to_meter(own.kinematics.altitude_ft) / 1000.0 - 10.0;   // km
    const double y = own.get_after_out_speed_kt() - 700.0;                         // kt

    const double r_cold_nm = std::exp(2.646  + 0.1523 * x - 0.00201 * y - 0.00008 * x * y);

    return 1.1 * r_cold_nm;   // güvenlik payı: ölçülen bütün noktaları kapsar
}


// NM, No Escape Zone: düşman füzesini biz dönüşe başladığımız an atarsa kaçamayacağımız mesafe.
// Füze dönüşümüz boyunca uçar; biz o sürede kaçamayız. Dönüş bitince kaçmaya başlarız.
//   NEZ = r_cold_nm + d + V_kaçış · Δt
//     r_cold_nm:          baştan kaçıyor olsaydık füzenin yetişeceği mesafe
//     d:               dönüşte düşmana yaklaşmamız
//     V_kaçış · Δt:    dönerken kaçamadığımız yol
double Engagement::nez_nm(const Aircraft& own, const Aircraft& enemy)
{
    const double cold_nm = r_cold_nm(own, enemy);
    const double approach_nm = own.get_out_approach_nm();

    const double turn_time_s = own.get_out_time_spend_sec();
    const double escape_kt = own.get_after_out_speed_kt();
    const double lost_nm = escape_kt * turn_time_s / SECONDS_PER_HOUR;

    return cold_nm + approach_nm + lost_nm;
}

// NM, Minimum Abort Range: dönüşün herhangi bir anında atılan füzeden kaçabileceğimiz en kısa mesafe.
// Düşman dönüş boyunca (0..Δt) herhangi bir anda atabilir. İki uç durum:
//   1) dönüş başında atar: füze Δt uçmuş, biz kaçamamışız      → r_cold_nm + d + V_kaçış · Δt   (= NEZ)
//   2) dönüş sonunda atar: düşman Δt boyunca bize yaklaşmış     → r_cold_nm + d + V_düşman · Δt
// Arası doğrusal; en kötüsü iki uçtan büyük olanı.
//   MAR = max(1, 2) + patlama yarıçapı + out payı
double Engagement::mar_nm(const Aircraft& own, const Aircraft& enemy)
{
    const double cold_nm = r_cold_nm(own, enemy);
    const double approach_nm = own.get_out_approach_nm();
    const double turn_time_s = own.get_out_time_spend_sec();

    // 1) Dönüş başında atış
    const double fire_at_start_nm = nez_nm(own, enemy);

    // 2) Dönüş sonunda atış: düşmanın bize doğru hız bileşeni kadar yaklaşma
    const double enemy_closed_nm = own.enemy.enemy_closing_kt * turn_time_s / SECONDS_PER_HOUR;
    const double fire_at_end_nm = cold_nm + approach_nm + enemy_closed_nm;

    const double worst_nm = std::max(fire_at_start_nm, fire_at_end_nm);

    // Patlama yarıçapı feet; diğer terimler NM.
    const double blast_nm = feet_to_nmile(enemy.missiles[0].profile.blast_radius_ft);

    return worst_nm + blast_nm + own.profile.out_offset_nm;
}

bool Engagement::is_in_mar(const Aircraft& own, const Aircraft& enemy)
{
    bool _is_in_mar = false;
    
    if(own.enemy.range_3d_nm <= mar_nm(own, enemy))
    {
        _is_in_mar = true;
    }

    return _is_in_mar;
}

double Engagement::dor_nm(const Aircraft& own, const Aircraft& enemy)
{
 return 0;
}

