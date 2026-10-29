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

// R_cold = s* - V_kaçış · τ*
//   s*, τ*: füzenin hızı V_kaçış'a düştüğünde uçtuğu yol ve geçen süre (glide_table)
// Tablo eşit adımlı: satır = (Mach - ilk Mach) / adım; iki satır arası doğrusal.
double Engagement::r_cold(const Aircraft& own, const Aircraft& enemy)
{
    const std::vector<MissileGlidePoint>& table = enemy.missiles[0].profile.glide_table_10km;

    // Kaçış hızımızı tablonun Mach'ına çevir.
    const double escape_mps = knot_to_mps(own.get_after_out_speed_kt());
    const double escape_mach = escape_mps / 299.5;

    const double lowest_mach = table.front().mach;    // eğrinin sonu
    const double highest_mach = table.back().mach;    // tepe hız
    const double mach_step = table[1].mach - table[0].mach;

     // Tepe hızdan hızlıysak füze hiç yetişemez.
    if (escape_mach >= highest_mach)
    {
        return 0.0;
    }

    double stop_distance_m = 0.0;
    double stop_time_s = 0.0;

    if (escape_mach <= lowest_mach)
    {
        // Füze bizim hızımıza inmeden ömrü bitti: eğrinin sonu.
        stop_distance_m = table.front().distance_m;
        stop_time_s = table.front().time_s;
    }
    else
    {
        // Hangi iki satırın arasındayız: tam kısmı alt satır, kesirli kısmı oran.
        const double position = (escape_mach - lowest_mach) / mach_step;
        const size_t lower_index = static_cast<size_t>(position);
        const double ratio = position - lower_index;

        const MissileGlidePoint& lower = table[lower_index];
        const MissileGlidePoint& upper = table[lower_index + 1];

        stop_distance_m = lower.distance_m + ratio * (upper.distance_m - lower.distance_m);
        stop_time_s = lower.time_s + ratio * (upper.time_s - lower.time_s);
    }

    // Füzenin yolu - bizim aynı sürede kaçtığımız yol.
    const double escape_distance_m = escape_mps * stop_time_s;
    const double stern_m = stop_distance_m - escape_distance_m;

    return meter_to_nmile(stern_m);
}

// NM, No Escape Zone: düşman füzesini biz dönüşe başladığımız an atarsa kaçamayacağımız mesafe.
// Füze dönüşümüz boyunca uçar; biz o sürede kaçamayız. Dönüş bitince kaçmaya başlarız.
//   NEZ = R_cold + d + V_kaçış · Δt
//     R_cold:          baştan kaçıyor olsaydık füzenin yetişeceği mesafe
//     d:               dönüşte düşmana yaklaşmamız
//     V_kaçış · Δt:    dönerken kaçamadığımız yol
double Engagement::nez_nm(const Aircraft& own, const Aircraft& enemy)
{
    const double cold_nm = r_cold(own, enemy);
    const double approach_nm = own.get_out_approach_nm();

    const double turn_time_s = own.get_out_time_spend_sec();
    const double escape_kt = own.get_after_out_speed_kt();
    const double lost_nm = escape_kt * turn_time_s / SECONDS_PER_HOUR;

    return cold_nm + approach_nm + lost_nm;
}

// NM, Minimum Abort Range: dönüşün herhangi bir anında atılan füzeden kaçabileceğimiz en kısa mesafe.
// Düşman dönüş boyunca (0..Δt) herhangi bir anda atabilir. İki uç durum:
//   1) dönüş başında atar: füze Δt uçmuş, biz kaçamamışız      → R_cold + d + V_kaçış · Δt   (= NEZ)
//   2) dönüş sonunda atar: düşman Δt boyunca bize yaklaşmış     → R_cold + d + V_düşman · Δt
// Arası doğrusal; en kötüsü iki uçtan büyük olanı.
//   MAR = max(1, 2) + patlama yarıçapı + out payı
double Engagement::mar_nm(const Aircraft& own, const Aircraft& enemy)
{
    const double cold_nm = r_cold(own, enemy);
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


double Engagement::dor_nm(const Aircraft& own, const Aircraft& enemy)
{
 return 0;
}
