#include "model/world.hpp"

#include "calculations/engagement.hpp"
#include "calculations/geometry.hpp"
#include "calculations/missile_geometry.hpp"

namespace
{
    void compute_missile_info(Aircraft& own, const Aircraft& target)   // own'un havadaki füzeleri: missile.target alanları; own atan uçak
    {
        for (Missile& missile : own.weapons.missiles)
        {
            if (missile.has_kinematics && missile.in_flight() && missile.target_id == target.id)   // atılmamış/bitmiş füzede son değerler kalır
            {
                MissileTargetInfo& info = missile.target;
                info.range_nm = missile_geometry::range_nm(missile, target);
                info.closure_kt = missile_geometry::closure_kt(missile, target);
                info.is_pitbull = info.is_pitbull || missile_geometry::is_pitbull(missile, info.range_nm);   // radar bir kez açılınca kapanmaz
                info.catch_time_left_s = missile_geometry::catch_time_left_s(missile);                      // range ve closure'dan sonra
                info.pitbull_time_left_s = missile_geometry::pitbull_time_left_s(missile);                  // is_pitbull'dan sonra
                info.is_guided = info.is_guided && missile_geometry::is_guided(missile, own, target);       // güdüm bir kez kopunca dönmez
            }
        }
    }

    std::vector<const Missile*> find_threats(const Aircraft& own, const Aircraft& enemy)   // enemy'nin bana uçan füzeleri; bilgileri missile->target'ta
    {
        std::vector<const Missile*> threats;
        for (const Missile& missile : enemy.weapons.missiles)
        {
            if (missile.has_kinematics && missile.in_flight() && missile.target_id == own.id)
            {
                threats.push_back(&missile);
            }
        }
        return threats;
    }

    const Missile* closest_threat(const std::vector<const Missile*>& threats)   // bana en yakın güdümlü tehdit; yoksa nullptr
    {
        const Missile* closest = nullptr;
        for (const Missile* threat : threats)
        {
            if (!threat->target.is_guided)
            {
                continue;   // güdümünü kaybetmiş füze tehdit sayılmaz (destek kuralı)
            }
            if (closest == nullptr || threat->target.range_nm < closest->target.range_nm)
            {
                closest = threat;
            }
        }
        return closest;
    }

    void compute_picture(Aircraft& own, const Aircraft& enemy)   // own.picture; sıra: geometri → kaçış → tehdit
    {
        const Kinematics& own_k = own.kinematics;
        const Kinematics& enemy_k = enemy.kinematics;
        Picture& picture = own.picture;

        picture.range_2d_nm = geometry::range_2d_nm(own_k, enemy_k);
        picture.range_3d_nm = geometry::range_3d_nm(own_k, enemy_k);
        picture.bearing_deg = geometry::bearing_deg(own_k, enemy_k);
        picture.ata_deg = geometry::ata_deg(own_k, enemy_k);
        picture.ta_deg = geometry::ta_deg(own_k, enemy_k);
        picture.los_nm = geometry::los_nm(own_k, enemy_k);
        picture.los_unit = geometry::los_unit(own_k, enemy_k);
        picture.closure_kt = geometry::closure_kt(own_k, enemy_k);
        picture.enemy_closure_kt = geometry::other_closure_kt(own_k, enemy_k);
        picture.los_rate_deg_s = geometry::los_rate_deg_s(own_k, enemy_k);
        picture.enemy_velocity_ned_kt = enemy_k.velocity_ned_kt;

        picture.out = engagement::out_plan(own, enemy);           // kaçış: düşmanın ana silahından
        picture.r_cold_nm = engagement::r_cold_nm(own, enemy);
        picture.nez_nm = engagement::nez_nm(own, enemy);
        picture.mar_nm = engagement::mar_nm(own, enemy);
        picture.is_in_mar = engagement::in_mar(own, enemy);
        picture.dr_nm = engagement::dr_nm(own, enemy);
        picture.dor_nm = engagement::dor_nm(own, enemy);
        picture.is_in_dor = engagement::in_dor(own, enemy);
        picture.mrr_nm = engagement::mrr_nm(own, enemy);
        picture.notch_turn_s = engagement::notch_turn_s(own);
        picture.enemy_out_turn_s = engagement::out_plan(enemy, own).turn.time_s;

        picture.threats = find_threats(own, enemy);
        picture.threat = closest_threat(picture.threats);
        picture.is_spiked = !picture.threats.empty();

        picture.threat_escape_margin_nm.reset();   // tehdit yoksa pay yok
        picture.threat_recommit_margin_nm.reset();
        if (picture.threat != nullptr)
        {
            const Missile& threat = *picture.threat;
            picture.threat_escape_margin_nm = missile_geometry::escape_margin_nm(threat, picture.out.turn.time_s, picture.out.escape_kt);
            picture.threat_recommit_margin_nm = missile_geometry::escape_margin_nm(threat, engagement::recommit_turn_s(own), -own.kinematics.tas_kt);
        }
    }

    void compute_weapons(Aircraft& own, const Aircraft& enemy)   // own.weapons'ın hesaplanan alanları; picture'dan sonra (pole'lar ve TR okur)
    {
        Weapons& weapons = own.weapons;

        weapons.wez = engagement::wez_nm(own, enemy);
        weapons.enemy_in_wez = engagement::in_wez(own, enemy);
        weapons.a_pole_nm = engagement::a_pole_nm(own, enemy);
        weapons.f_pole_nm = engagement::f_pole_nm(own, enemy);
        weapons.tr_nm = engagement::tr_nm(own, enemy);
        weapons.enemy_in_tr = engagement::in_tr(own, enemy);
        weapons.msr_nm = engagement::msr_nm(own, enemy);
        weapons.enemy_in_msr = engagement::in_msr(own, enemy);
        weapons.enemy_in_nez = engagement::in_nez(own, enemy);
    }
}

// Hesap sırası:
//   1. füze bilgileri: tehdit seçimi karşı tarafın füze bilgisini kullanır
//   2. picture, 3. weapons: pole'lar ve TR picture'ı okur
//   4. karşı tarafın değerlerinin kopyası: ikisi de bu step hesaplandıktan sonra; önce kopyalansa önceki step'in değeri gelirdi
void World::update()
{
    fighter.enemy_id = hostile.id;   // 1v1: düşmanlar sabit; çok uçakta düşman seçimi yalnız burada değişir
    hostile.enemy_id = fighter.id;

    compute_missile_info(fighter, hostile);
    compute_missile_info(hostile, fighter);

    compute_picture(fighter, hostile);
    compute_picture(hostile, fighter);

    compute_weapons(fighter, hostile);
    compute_weapons(hostile, fighter);

    fighter.picture.me_in_enemy_wez = hostile.weapons.enemy_in_wez;   // "ben onun WEZ'inde miyim" = onun "düşman WEZ'imde mi"
    hostile.picture.me_in_enemy_wez = fighter.weapons.enemy_in_wez;
    fighter.picture.enemy_a_pole_nm = hostile.weapons.a_pole_nm;       // düşman şimdi atsa
    hostile.picture.enemy_a_pole_nm = fighter.weapons.a_pole_nm;
    fighter.picture.enemy_f_pole_nm = hostile.weapons.f_pole_nm;
    hostile.picture.enemy_f_pole_nm = fighter.weapons.f_pole_nm;
    fighter.weapons.r_cold_nm = hostile.picture.r_cold_nm;             // benim R_cold'um = düşmanın benden kaçarken R_cold'u
    hostile.weapons.r_cold_nm = fighter.picture.r_cold_nm;
}
