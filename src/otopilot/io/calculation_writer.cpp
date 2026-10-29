#include "io/calculation_writer.hpp"

Json CalculationWriter::write(const World& world)
{
    Json data;
    data["aircraft"] = Json::array({to_json(world.fighter, world.hostile), to_json(world.hostile, world.fighter)});
    return data;
}

Json CalculationWriter::to_json(const Aircraft& own, const Aircraft& target)
{
    // Anahtar adları CSV sütun adı olur; birim adın sonunda (_nm, _kt, _deg).
    Json j;
    j["id"] = own.id;
    j["target_id"] = target.id;
    j["phase"] = static_cast<int>(own.phase);   // 0 Hold, 1 Fire, 2 Crank, 3 Out, 4 o

    // Hesaplanan: World::update (EnemyInfo)
    j["range_2d_nm"] = own.enemy.range_2d_nm;
    j["range_3d_nm"] = own.enemy.range_3d_nm;
    j["bearing_deg"] = own.enemy.bearing_deg;
    j["ata_deg"] = own.enemy.ata_deg;
    j["aspect_deg"] = own.enemy.aspect_deg;
    j["los_north_nm"] = own.enemy.los_nm.north;
    j["los_east_nm"] = own.enemy.los_nm.east;
    j["los_down_nm"] = own.enemy.los_nm.down;
    j["rate_of_closure_kt"] = own.enemy.rate_of_closure_kt;
    j["enemy_closing_kt"] = own.enemy.enemy_closing_kt;
    j["enemy_in_my_wez"] = own.enemy.enemy_in_my_wez;
    j["me_in_enemy_wez"] = own.enemy.me_in_enemy_wez;
    j["stern_wez_nm"] = own.enemy.stern_wez_nm;
    j["nez_nm"] = own.enemy.nez_nm;
    j["mar_nm"] = own.enemy.mar_nm;
    j["dor_nm"] = own.enemy.dor_nm;

    // Sabit: profil. Görüntüleyici radar açısını ve WEZ'i bunlarla çizer.
    j["radar_gimbal_deg"] = own.profile.radar_gimbal_deg;   // yarım açı (60 = ±60)
    j["radar_range_nm"] = own.profile.radar_range_nm;
    j["wez_min_range_nm"] = own.enemy.my_wez.min_nm;   // ilk füzeye göre, bu step'in WEZ'i
    j["wez_max_range_nm"] = own.enemy.my_wez.max_nm;   // düşmanın yönüne göre değişir
    return j;
}
