#include "model/world.hpp"

#include "calculations/engagement.hpp"
#include "calculations/geometry.hpp"
#include "calculations/missile_geometry.hpp"

namespace
{
    // own'un havadaki füzeleri için kendi hedeflerine göre bilgiler: missile.target alanlarını doldurur.
    void compute_missile_info(Aircraft& own, const Aircraft& target)
    {
        for (Missile& missile : own.weapons.missiles)
        {
            // Atılmamış, henüz eşleşmemiş ya da bitmiş füzede son değerler kalır.
            if (missile.has_kinematics && missile.in_flight() && missile.target_id == target.id)
            {
                MissileTargetInfo& info = missile.target;
                info.range_nm = missile_geometry::range_nm(missile, target);
                info.closure_kt = missile_geometry::closure_kt(missile, target);
                info.time_to_go_s = missile_geometry::time_to_go_s(info.range_nm, info.closure_kt);

                // Füze radarını bir kez açınca kapatmaz; hedef sonra uzaklaşsa da pitbull kalır.
                info.is_pitbull = info.is_pitbull || missile_geometry::is_pitbull(missile, info.range_nm);
            }
        }
    }

    // candidate, şu ana kadar seçilen tehditten daha mı acil?
    //   Kalan süresi kısa olan daha acildir. Yaklaşmayan füzenin (süresi yok) önceliği en sondadır.
    bool is_more_urgent(const ThreatInfo& candidate, const std::optional<ThreatInfo>& current)
    {
        if (!current.has_value())
        {
            return true;    // henüz seçilen yok
        }
        if (!candidate.time_to_go_s.has_value())
        {
            return false;   // aday yaklaşmıyor
        }
        if (!current->time_to_go_s.has_value())
        {
            return true;    // seçilen yaklaşmıyor, aday yaklaşıyor
        }
        return *candidate.time_to_go_s < *current->time_to_go_s;
    }

    // enemy'nin bana doğru uçan füzelerinden en acil olanı.
    std::optional<ThreatInfo> find_threat(const Aircraft& own, const Aircraft& enemy)
    {
        std::optional<ThreatInfo> threat;
        for (const Missile& missile : enemy.weapons.missiles)
        {
            if (!missile.has_kinematics || !missile.in_flight() || missile.target_id != own.id)
            {
                continue;
            }

            ThreatInfo candidate;
            candidate.slot = missile.slot;
            candidate.range_nm = missile.target.range_nm;
            candidate.closure_kt = missile.target.closure_kt;
            candidate.time_to_go_s = missile.target.time_to_go_s;
            candidate.bearing_deg = geometry::bearing_deg(own.kinematics, missile.kinematics);
            candidate.ata_deg = geometry::ata_deg(own.kinematics, missile.kinematics);
            candidate.aspect_deg = geometry::aspect_angle_deg(own.kinematics, missile.kinematics);
            candidate.mach = missile.kinematics.mach;
            candidate.is_pitbull = missile.target.is_pitbull;

            if (is_more_urgent(candidate, threat))
            {
                threat = candidate;
            }
        }
        return threat;
    }

    // own'un gözünden enemy: own.picture'ı doldurur. Sıra önemli: geometri → out planı → kaçış → tehdit.
    void compute_picture(Aircraft& own, const Aircraft& enemy)
    {
        const Kinematics& own_k = own.kinematics;
        const Kinematics& enemy_k = enemy.kinematics;
        Picture& picture = own.picture;

        picture.range_2d_nm = geometry::range_2d_nm(own_k, enemy_k);
        picture.range_3d_nm = geometry::range_3d_nm(own_k, enemy_k);
        picture.bearing_deg = geometry::bearing_deg(own_k, enemy_k);
        picture.ata_deg = geometry::ata_deg(own_k, enemy_k);
        picture.aspect_deg = geometry::aspect_angle_deg(own_k, enemy_k);
        picture.los_nm = geometry::los_nm(own_k, enemy_k);
        picture.los_unit = geometry::los_unit(own_k, enemy_k);
        picture.closure_kt = geometry::closure_kt(own_k, enemy_k);
        picture.enemy_closure_kt = geometry::other_closure_kt(own_k, enemy_k);

        // Kaçış: düşmanın ana silahından.
        picture.out = engagement::out_plan(own, enemy);
        picture.r_cold_nm = engagement::r_cold_nm(own, enemy);
        picture.nez_nm = engagement::nez_nm(own, enemy);
        picture.mar_nm = engagement::mar_nm(own, enemy);
        picture.is_in_mar = engagement::in_mar(own, enemy);
        picture.dor_nm = engagement::dor_nm(own, enemy);

        picture.threat = find_threat(own, enemy);
    }

    // own'un ana silahının enemy'ye karşı durumu: own.weapons'ın hesaplanan alanları. own.picture'dan sonra çağrılır.
    void compute_weapons(Aircraft& own, const Aircraft& enemy)
    {
        const Picture& picture = own.picture;
        Weapons& weapons = own.weapons;

        weapons.wez = engagement::wez(own, enemy);
        weapons.enemy_in_wez = engagement::in_wez(own, enemy);
        weapons.f_pole_nm = engagement::f_pole_nm(own, enemy);

        // NEZ: düşman benden out yapsa füzemden kaçamayacağı mesafe = düşmanın gözünden kaçış NEZ'i (uçaklar ters).
        weapons.nez_nm = engagement::nez_nm(enemy, own);
        weapons.enemy_in_nez = picture.range_3d_nm > weapons.wez.min_nm && picture.range_3d_nm <= weapons.nez_nm;
    }
}

void World::update()
{
    // 1v1: düşmanlar sabit. Çok uçakta düşman seçimi yalnız burada değişir.
    fighter.enemy_id = hostile.id;
    hostile.enemy_id = fighter.id;

    // Füze bilgileri önce: tehdit seçimi karşı tarafın füze bilgisini kullanır.
    compute_missile_info(fighter, hostile);
    compute_missile_info(hostile, fighter);

    compute_picture(fighter, hostile);
    compute_picture(hostile, fighter);

    compute_weapons(fighter, hostile);
    compute_weapons(hostile, fighter);

    // "Ben onun WEZ'inde miyim" = onun hesabındaki "düşman benim WEZ'imde mi".
    // İkisi de bu step hesaplandıktan sonra kopyalanır; önce kopyalansa önceki step'in değeri gelirdi.
    fighter.picture.me_in_enemy_wez = hostile.weapons.enemy_in_wez;
    hostile.picture.me_in_enemy_wez = fighter.weapons.enemy_in_wez;
}
