#include "world.hpp"
#include "geometry.hpp"
#include "weapons.hpp"

// Kendi füze listemden: kaç hazır füzem var, kaçı havada. Yalnız bu dosyada (static).
static void compute_own_info(const Aircraft& own, OwnInfo& info)
{
    // Nesne step'ler arasında yaşıyor; önceki step'in sayısının üstüne eklememek için sıfırla.
    info.ready_missiles = 0;
    info.missile_in_flight = 0;

    for (const Missile& m : own.missiles)
    {
        if (m.state == MissileState::Ready)
        {
            info.ready_missiles += 1;
        }
        if (m.state == MissileState::InFlight)
        {
            info.missile_in_flight += 1;
        }
    }
}

// own'un gözünden target: own.enemy alanlarını doldurur. Yalnız bu dosyada (static).
static void fill_enemy_info(Aircraft& own, const Aircraft& target)
{
    // Önce saf geometri ...
    own.enemy.range_2d_m  = Geometry::range_2d_m(own.kinematics, target.kinematics);
    own.enemy.range_3d_m  = Geometry::range_3d_m(own.kinematics, target.kinematics);
    own.enemy.bearing_deg = Geometry::bearing_deg(own.kinematics, target.kinematics);
    own.enemy.ata_deg     = Geometry::ATA_deg(own.kinematics, target.kinematics);

    // ... sonra silah: Weapons, yukarıda dolan range'i own.enemy'den okuyabilir.
    own.enemy.in_wez = Weapons::in_wez(own, target);
    own.enemy.mar_m  = Weapons::mar_m(own, target);
    own.enemy.dor_m  = Weapons::dor_m(own, target);
}


void World::update()
{
    compute_own_info(fighter, fighter.own);
    compute_own_info(bogey, bogey.own);

    fill_enemy_info(fighter, bogey);   // bizim bogey'e bakışımız
    fill_enemy_info(bogey, fighter);   // bogey'in bize bakışı

}
