#include "model/weapons.hpp"

#include <algorithm>

bool Weapons::fire()
{
    if (has_launch_order || missiles_left() == 0)
    {
        return false;
    }
    has_launch_order = true;
    return true;
}

void Weapons::record_launch(Missile& missile)
{
    int launched = 0;
    for (const Missile& other : missiles)
    {
        if (other.fire_order > 0)
        {
            launched += 1;
        }
    }
    missile.fire_order = launched + 1;
    missile.target.a_pole_nm = a_pole_nm;   // önceki step'in, yani emrin verildiği anın tahmini; uçuşta güncellenmez
    missile.target.f_pole_nm = f_pole_nm;
}

int Weapons::missiles_left() const
{
    int count = 0;
    for (const Missile& missile : missiles)
    {
        if (missile.state == MissileState::Ready)
        {
            count += 1;
        }
    }
    if (has_launch_order)
    {
        count -= 1;   // emir verildi: sim bu step atar
    }
    return count;
}

// Aktif: havada ve kilitli (sim is_locked: datalink ya da seeker). Kilitsiz füze sayılmaz.
// Slotlar atış sırasında değil; aktifler toplanıp fire_order'a göre sıralanır (en çok slot sayısı kadar).
std::vector<const Missile*> Weapons::active_missiles() const
{
    std::vector<const Missile*> active;
    for (const Missile& missile : missiles)
    {
        if (missile.in_flight() && missile.target.is_guided)
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
    const Missile* latest = nullptr;
    for (const Missile& missile : missiles)
    {
        const bool active = missile.in_flight() && missile.target.is_guided;   // active_missiles ile aynı ölçüt
        if (active && (latest == nullptr || missile.fire_order > latest->fire_order))
        {
            latest = &missile;
        }
    }
    return latest;
}
