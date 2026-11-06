#include "model/weapons.hpp"

#include <algorithm>

bool Weapons::fire(const MissileProfile& profile)
{
    int fired_count = 0;
    for (const Missile& missile : missiles)
    {
        if (missile.is_fired)
        {
            fired_count += 1;
            if (missile.python_id.empty())
            {
                return false;   // emri verilmiş ama henüz uçmamış: ikinci emir eşleştirmeyi kaydırırdı
            }
        }
    }

    for (Missile& missile : missiles)
    {
        if (!missile.is_fired && missile.profile.name == profile.name)
        {
            missile.is_fired = true;
            missile.fire_order = fired_count + 1;
            missile.target.a_pole_nm = a_pole_nm;   // atış anındaki tahmin; uçuşta güncellenmez
            missile.target.f_pole_nm = f_pole_nm;
            has_launch_order = true;
            return true;
        }
    }

    return false;   // o profilde atılmamış füze kalmadı
}

int Weapons::missiles_left() const
{
    int count = 0;
    for (const Missile& missile : missiles)
    {
        if (!missile.is_fired)
        {
            count += 1;
        }
    }
    return count;
}

// Slotlar atış sırasında değil; havadakiler toplanıp fire_order'a göre sıralanır (en çok slot sayısı kadar).
std::vector<const Missile*> Weapons::active_missiles() const
{
    std::vector<const Missile*> active;
    for (const Missile& missile : missiles)
    {
        if (missile.in_flight())
        {
            active.push_back(&missile);
        }
    }
    std::sort(active.begin(), active.end(),
              [](const Missile* a, const Missile* b) { return a->fire_order < b->fire_order; });
    return active;
}

const Missile* Weapons::active_missile() const
{
    const Missile* active = nullptr;
    for (const Missile& missile : missiles)
    {
        if (missile.in_flight() && (active == nullptr || missile.fire_order > active->fire_order))
        {
            active = &missile;
        }
    }
    return active;
}
