#include "model/aircraft.hpp"
#include <algorithm>
#include <cmath>

double Aircraft::get_turn_radius_nm() const
{
    // Hızı knot'tan m/s'ye çevir.
    const double speed_mps = knot_to_mps(kinematics.tas_kt);

    // Out manevrasında uygulanacağını varsaydığımız yük faktörü.
    const double n = profile.out_g;

    // Yatay ivme: g * sqrt(n² - 1)
    // Dönüş yarıçapı: hız² / yatay ivme
    const double radius_m = (speed_mps * speed_mps) /
        (GRAVITY_MPS2 * std::sqrt(n * n - 1.0));

    return meter_to_nmile(radius_m);
}

double Aircraft::get_out_distance_nm(const Aircraft& enemy) const
{
    (void)enemy;
    return 0.0;
}

double Aircraft::get_in_distance_nm(const Aircraft& enemy) const
{
   (void)enemy;
   return 0.0;
}

bool Aircraft::fire_missile(const MissileProfile& type)
{
    // Emri verilmiş ama Python'un henüz uçurmadığı bir füzemiz varken yenisini verme:
    // iki emir aynı step'te giderse tek launch gider, eşleştirme kayar. Havadaki füze engel değil.
    int fired_count = 0;
    for (const Missile& m : missiles)
    {
        if (m.fired)
        {
            fired_count += 1;
            if (m.python_id.empty())
            {
                return false;   // emri verilmiş ama henüz uçmamış
            }
        }
    }

    for (Missile& m : missiles)
    {
        if (!m.fired && m.profile.name == type.name)
        {
            m.fired = true;
            m.fire_order = fired_count + 1;
            launch_order = true;
            return true;
        }
    }

    return false;   // o tipte atılmamış füze kalmadı
}

int Aircraft::missiles_left() const
{
    int count = 0;
    for (const Missile& m : missiles)
    {
        if (!m.fired)
        {
            count += 1;
        }
    }
    return count;
}

bool Aircraft::has_active_missile_against(const Aircraft& target) const
{
    for (const Missile& missile : missiles)
    {
        if (missile.in_flight() && missile.target_id == target.id)
        {
            return true;
        }
    }

    return false;
}

const Missile* Aircraft::last_fired() const
{
    const Missile* last = nullptr;
    for (const Missile& m : missiles)
    {
        if (m.fired && (last == nullptr || m.fire_order > last->fire_order))
        {
            last = &m;
        }
    }
    return last;
}
