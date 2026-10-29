#include "model/world.hpp"
#include "calculations/geometry.hpp"
#include "calculations/missile_geometry.hpp"
#include "calculations/engagement.hpp"

// own'un gözünden target: own.enemy alanlarını doldurur. Yalnız bu dosyada (static).
static void read_enemy_info(Aircraft& own, const Aircraft& target)
{
    // Önce saf geometri ...
    own.enemy.range_2d_nm  = Geometry::range_2d_nm(own.kinematics, target.kinematics);
    own.enemy.range_3d_nm  = Geometry::range_3d_nm(own.kinematics, target.kinematics);
    own.enemy.bearing_deg = Geometry::bearing_deg(own.kinematics, target.kinematics);
    own.enemy.ata_deg     = Geometry::ata_deg(own.kinematics, target.kinematics);
    own.enemy.aspect_deg  = Geometry::aspect_angle_deg(own.kinematics, target.kinematics);
    own.enemy.los_nm       = Geometry::los_nm(own.kinematics, target.kinematics);
    own.enemy.los_unit    = Geometry::los_unit(own.kinematics, target.kinematics);
    own.enemy.rate_of_closure_kt       = Geometry::rate_of_closure_kt(own.kinematics, target.kinematics);

    // ... sonra silah: Engagement, yukarıda dolan geometriyi own.enemy'den okur.
    own.enemy.my_wez          = Engagement::wez(own);
    own.enemy.enemy_in_my_wez = Engagement::in_wez(own.enemy.range_3d_nm, own.enemy.my_wez);
    own.enemy.stern_wez_nm     = Engagement::r_cold(own, target);
    own.enemy.nez_nm           = Engagement::nez_nm(own, target);
    own.enemy.mar_nm           = Engagement::mar_nm(own, target);
    own.enemy.dor_nm           = Engagement::dor_nm(own, target);
}

// own'un havadaki füzeleri için target'a göre bilgiler: missile.target alanlarını doldurur.
static void read_missile_info(Aircraft& own, const Aircraft& target)
{
    for (Missile& m : own.missiles)
    {
        // Atılmamış, henüz eşleşmemiş ya da bitmiş füzede son değerler kalır.
        if (m.has_kinematics && m.in_flight() && m.target_id == target.id)
        {
            m.target.range_nm      = MissileGeometry::range_nm(m, target);
            m.target.closure_kt  = MissileGeometry::closure_kt(m, target);
            m.target.time_to_go_s = MissileGeometry::time_to_go_s(m.target.range_nm, m.target.closure_kt);

            // Füze radarını bir kez açınca kapatmaz; hedef sonra uzaklaşsa da pitbull kalır.
            m.target.pitbull = m.target.pitbull || MissileGeometry::pitbull(m, m.target.range_nm);
        }
    }
}


void World::update()
{
    read_enemy_info(fighter, hostile);   // bizim hostile'e bakışımız
    read_enemy_info(hostile, fighter);   // hostile'in bize bakışı

    read_missile_info(fighter, hostile);   // bizim füzelerimiz hostile'e göre
    read_missile_info(hostile, fighter);   // hostile'in füzeleri bize göre

    // "Ben onun WEZ'inde miyim" = onun hesabındaki "düşman benim WEZ'imde mi".
    // İkisi de bu step hesaplandıktan sonra kopyalanır; önce kopyalansa önceki step'in değeri gelirdi.
    fighter.enemy.me_in_enemy_wez = hostile.enemy.enemy_in_my_wez;
    hostile.enemy.me_in_enemy_wez = fighter.enemy.enemy_in_my_wez;
}
