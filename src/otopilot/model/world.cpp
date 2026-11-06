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

                // Füze radarını bir kez açınca kapatmaz; hedef sonra uzaklaşsa da pitbull kalır.
                info.is_pitbull = info.is_pitbull || missile_geometry::is_pitbull(missile, info.range_nm);

                // range_nm ve closure_kt'den sonra: hesap güncel mesafe ve kapanmayı kullanır.
                info.catch_time_left_s = missile_geometry::catch_time_left_s(missile);
            }
        }
    }

    // enemy'nin bana doğru uçan tüm füzelerinin adresleri. Füzenin bana göre bilgileri missile->target'ta.
    std::vector<const Missile*> find_threats(const Aircraft& own, const Aircraft& enemy)
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

    // Tehditlerden bana en yakın olanı; füze yoksa nullptr.
    const Missile* closest_threat(const std::vector<const Missile*>& threats)
    {
        const Missile* closest = nullptr;
        for (const Missile* threat : threats)
        {
            if (closest == nullptr || threat->target.range_nm < closest->target.range_nm)
            {
                closest = threat;
            }
        }
        return closest;
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
        picture.ta_deg = geometry::ta_deg(own_k, enemy_k);
        picture.los_nm = geometry::los_nm(own_k, enemy_k);
        picture.los_unit = geometry::los_unit(own_k, enemy_k);
        picture.closure_kt = geometry::closure_kt(own_k, enemy_k);
        picture.enemy_closure_kt = geometry::other_closure_kt(own_k, enemy_k);
        picture.los_rate_deg_s = geometry::los_rate_deg_s(own_k, enemy_k);

        // Kaçış: düşmanın ana silahından.
        picture.out = engagement::out_plan(own, enemy);
        picture.r_cold_nm = engagement::r_cold_nm(own, enemy);
        picture.nez_nm = engagement::nez_nm(own, enemy);
        picture.mar_nm = engagement::mar_nm(own, enemy);
        picture.is_in_mar = engagement::in_mar(own, enemy);
        picture.dr_nm = engagement::dr_nm(own, enemy);
        picture.dor_nm = engagement::dor_nm(own, enemy);
        picture.is_in_dor = engagement::in_dor(own, enemy);

        picture.threats = find_threats(own, enemy);
        picture.threat = closest_threat(picture.threats);
        picture.is_spiked = !picture.threats.empty();
    }

    // own'un ana silahının enemy'ye karşı durumu: own.weapons'ın hesaplanan alanları. own.picture'dan sonra çağrılır.
    void compute_weapons(Aircraft& own, const Aircraft& enemy)
    {
        Weapons& weapons = own.weapons;

        weapons.wez = engagement::wez_nm(own, enemy);
        weapons.enemy_in_wez = engagement::in_wez(own, enemy);
        weapons.a_pole_nm = engagement::a_pole_nm(own, enemy);
        weapons.f_pole_nm = engagement::f_pole_nm(own, enemy);
        weapons.tr_nm = engagement::tr_nm(own, enemy);

        // NEZ: düşman benden out yapsa füzemden kaçamayacağı mesafe = düşmanın gözünden kaçış NEZ'i (uçaklar ters).
        weapons.nez_nm = engagement::nez_nm(enemy, own);
        weapons.enemy_in_nez = engagement::in_nez(own, enemy);
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

    // "Ben onun MAR'ının içinde miyim" = onun picture.is_in_mar'ı (menzil iki taraftan aynı).
    fighter.picture.me_in_enemy_mar = hostile.picture.is_in_mar;
    hostile.picture.me_in_enemy_mar = fighter.picture.is_in_mar;

    // Düşmanın "şimdi atsa" poleları: onun weapons hesabından, aynı sebeple compute_weapons'tan sonra.
    fighter.picture.enemy_a_pole_nm = hostile.weapons.a_pole_nm;
    hostile.picture.enemy_a_pole_nm = fighter.weapons.a_pole_nm;
    fighter.picture.enemy_f_pole_nm = hostile.weapons.f_pole_nm;
    hostile.picture.enemy_f_pole_nm = fighter.weapons.f_pole_nm;
}
