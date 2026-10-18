#include "weapons.hpp"

bool Weapons::in_wez(const Aircraft& own, const Aircraft& enemy)
{
    // TODO. Elindekiler:
    //   own.enemy.range_3d_m, own.profile.wez_min_range_m, own.profile.wez_max_range_m
    (void)own;
    (void)enemy;
    return false;
}

double Weapons::mar_m(const Aircraft& own, const Aircraft& enemy)
{
    // TODO. Düşmanın füze menzili (enemy.profile), benim hızım, kapanma ...
    (void)own;
    (void)enemy;
    return 0.0;
}

double Weapons::dor_m(const Aircraft& own, const Aircraft& enemy)
{
    // TODO
    (void)own;
    (void)enemy;
    return 0.0;
}
