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

double Engagement::mar_nm(const Aircraft& own, const Aircraft& enemy)
{
    // Patlama yarıçapı feet; toplamın diğer bütün terimleri NM'dir.
    const double blast_nm = enemy.missiles.empty()
        ? 0.0 : feet_to_nmile(enemy.missiles[0].profile.blast_radius_ft);
    return enemy.enemy.my_wez.max_nm + own.get_out_distance_nm(enemy)
        + blast_nm + own.profile.out_offset_nm;
}

double Engagement::dor_nm(const Aircraft& own, const Aircraft& enemy)
{
    return (own.enemy.mar_nm + own.get_in_distance_nm(enemy));
}
