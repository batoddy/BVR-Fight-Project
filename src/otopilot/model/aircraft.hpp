#pragma once

// Uçak verisi. fighter ve hostile aynı Aircraft yapısını kullanır.
// JSON bilmez. Alanların kim tarafından yazıldığı yorumlarda: KOD_STANDARTLARI.md §1.

#include <string>

#include "model/aircraft_profiles.hpp"
#include "model/flight_command.hpp"
#include "model/kinematics.hpp"
#include "model/phase.hpp"
#include "model/picture.hpp"
#include "model/weapons.hpp"

struct Aircraft
{
    // Kimlik | StateReader
    std::string id;      // "blue" / "red" | scenario.yaml agents anahtarı
    std::string name;    // görünen ad     | scenario.yaml name
    std::string team;    // "Blue" / "Red" | scenario.yaml team
    std::string type;    // profil adı     | scenario.yaml type
    double health = 0;   // 0..1, 1 sağlam | BVRGym agent.healthPoints

    AircraftProfile profile;   // sabit: type ile aircraft_profiles.hpp tablosundan | StateReader (tip değişince)
    Kinematics kinematics;     // ölçüm                                              | StateReader

    std::string enemy_id;   // atanmış düşmanın id'si                           | World::update
    Picture picture;        // atanmış düşmana göre geometri ve kaçış           | World::update
    Weapons weapons;        // füzeler | StateReader, fire; füzemin düşmana karşı durumu | World::update

    // Karar çıktısı
    FlightCommand command;       // her step Python'un son uyguladığı komutla başlar (StateReader); otopilot değiştirir
    Phase phase = Phase::Hold;   // şu an hangi aşamada | otopilot
};
