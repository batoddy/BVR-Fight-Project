#include "io/calculation_writer.hpp"

#include <optional>
#include <string>

#include "model/missile.hpp"

Json CalculationWriter::write(const World& world)
{
    Json data;
    data["aircraft"] = Json::array({to_json(world.fighter, world.hostile), to_json(world.hostile, world.fighter)});
    return data;
}

// Anahtar adları CSV sütun adı olur; birim adın sonunda (_nm, _kt, _deg). Sütunlar her step aynı kalmalı (recorder.py).
// Tel biçimi şimdilik eski adlarla: aspect_deg, rate_of_closure_kt, enemy_closing_kt, stern_wez_nm. Değeri olmayan hesap 0 yazılır.
Json CalculationWriter::to_json(const Aircraft& own, const Aircraft& enemy)
{
    const Picture& picture = own.picture;

    Json json;
    json["id"] = own.id;
    json["target_id"] = enemy.id;
    json["phase"] = static_cast<int>(own.phase);   // sayı karşılıkları model/phase.hpp'de

    json["range_2d_nm"] = picture.range_2d_nm;
    json["range_3d_nm"] = picture.range_3d_nm;
    json["bearing_deg"] = picture.bearing_deg;
    json["ata_deg"] = picture.ata_deg;
    json["aspect_deg"] = picture.ta_deg;   // tel adı değişmedi: görüntüleyici ve eski kayıtlar bunu okur
    json["los_north_nm"] = picture.los_nm.north;
    json["los_east_nm"] = picture.los_nm.east;
    json["los_down_nm"] = picture.los_nm.down;
    json["rate_of_closure_kt"] = picture.closure_kt;
    json["enemy_closing_kt"] = picture.enemy_closure_kt;
    json["enemy_in_my_wez"] = own.weapons.enemy_in_wez;
    json["me_in_enemy_wez"] = picture.me_in_enemy_wez;
    json["stern_wez_nm"] = picture.r_cold_nm;
    json["nez_nm"] = picture.nez_nm;
    json["mar_nm"] = picture.mar_nm;
    json["dr_nm"] = picture.dr_nm;
    json["dor_nm"] = picture.dor_nm;
    json["a_pole_nm"] = own.weapons.a_pole_nm.value_or(0.0);
    json["f_pole_nm"] = own.weapons.f_pole_nm.value_or(0.0);
    json["tr_nm"] = own.weapons.tr_nm.value_or(0.0);

    const Missile* threat = picture.threat;                                    // füze yoksa 0
    json["threat_range_nm"] = threat ? threat->target.range_nm : 0.0;
    json["threat_mach"] = threat ? threat->kinematics.mach : 0.0;
    json["threat_pitbull"] = (threat && threat->target.is_pitbull) ? 1 : 0;   // 0/1: okuyucu sayı bekler

    json["radar_gimbal_deg"] = own.profile.radar_gimbal_deg;   // görüntüleyici radar konisini çizer; yarım açı (60 = ±60)
    json["radar_range_nm"] = own.profile.radar_range_nm;
    json["wez_min_range_nm"] = own.weapons.wez.min_nm;         // main_weapon'a göre, bu step'in WEZ'i
    json["wez_max_range_nm"] = own.weapons.wez.max_nm;         // düşmanın yönüne göre değişir

    Json pitbull_slots = Json::array();                        // tek metin alanı: mühimmat sayısı değişse de sütunlar sabit
    for (const Missile& missile : own.weapons.missiles)
    {
        if (missile.target.is_pitbull)
        {
            pitbull_slots.push_back(missile.slot);
        }
    }
    json["pitbull_missile_slots"] = pitbull_slots.dump();

    Json catch_times = Json::object();                         // havadaki füzelerimin kalan süresi {"slot": s}; değeri yoksa yazılmaz
    for (const Missile& missile : own.weapons.missiles)
    {
        if (missile.in_flight() && missile.target.catch_time_left_s.has_value())
        {
            catch_times[std::to_string(missile.slot)] = *missile.target.catch_time_left_s;
        }
    }
    json["missile_catch_times_s"] = catch_times.dump();
    return json;
}
