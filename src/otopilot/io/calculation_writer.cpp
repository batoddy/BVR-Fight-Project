#include "io/calculation_writer.hpp"

#include <optional>

Json CalculationWriter::write(const World& world)
{
    Json data;
    data["aircraft"] = Json::array({to_json(world.fighter, world.hostile), to_json(world.hostile, world.fighter)});
    return data;
}

Json CalculationWriter::to_json(const Aircraft& own, const Aircraft& enemy)
{
    // Anahtar adları CSV sütun adı olur; birim adın sonunda (_nm, _kt, _deg).
    // Tel biçimi şimdilik eski adlarla: rate_of_closure_kt, enemy_closing_kt, stern_wez_nm.
    // Değeri olmayan (optional) hesaplar 0 yazılır.
    const Picture& picture = own.picture;

    Json json;
    json["id"] = own.id;
    json["target_id"] = enemy.id;
    json["phase"] = static_cast<int>(own.phase);   // model/phase.hpp: 0 Hold, 1 Fire, 2 Out, 3 In, 4 Crank, 5 Beam

    json["range_2d_nm"] = picture.range_2d_nm;
    json["range_3d_nm"] = picture.range_3d_nm;
    json["bearing_deg"] = picture.bearing_deg;
    json["ata_deg"] = picture.ata_deg;
    json["aspect_deg"] = picture.aspect_deg;
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
    json["dor_nm"] = picture.dor_nm;

    // Sabit: profil. Görüntüleyici radar açısını ve WEZ'i bunlarla çizer.
    json["radar_gimbal_deg"] = own.profile.radar_gimbal_deg;   // yarım açı (60 = ±60)
    json["radar_range_nm"] = own.profile.radar_range_nm;
    json["wez_min_range_nm"] = own.weapons.wez.min_nm;         // main_weapon'a göre, bu step'in WEZ'i
    json["wez_max_range_nm"] = own.weapons.wez.max_nm;         // düşmanın yönüne göre değişir
    return json;
}
