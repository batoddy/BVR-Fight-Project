#include "model/weapons.hpp"

bool Weapons::fire(const MissileProfile& profile)
{
    // Emri verilmiş ama Python'un henüz uçurmadığı bir füzemiz varken yenisini verme:
    // iki emir aynı step'te giderse tek launch gider, eşleştirme kayar. Havadaki füze engel değil.
    int fired_count = 0;
    for (const Missile& missile : missiles)
    {
        if (missile.is_fired)
        {
            fired_count += 1;
            if (missile.python_id.empty())
            {
                return false;   // emri verilmiş ama henüz uçmamış
            }
        }
    }

    for (Missile& missile : missiles)
    {
        if (!missile.is_fired && missile.profile.name == profile.name)
        {
            missile.is_fired = true;
            missile.fire_order = fired_count + 1;
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

bool Weapons::has_active_missile_against(const std::string& target_id) const
{
    for (const Missile& missile : missiles)
    {
        if (missile.in_flight() && missile.target_id == target_id)
        {
            return true;
        }
    }
    return false;
}

const Missile* Weapons::last_fired() const
{
    const Missile* last = nullptr;
    for (const Missile& missile : missiles)
    {
        if (missile.is_fired && (last == nullptr || missile.fire_order > last->fire_order))
        {
            last = &missile;
        }
    }
    return last;
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
