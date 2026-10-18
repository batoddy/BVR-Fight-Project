#pragma once

#include "maneuver.hpp"
#include "messages.hpp"
#include "orders.hpp"
#include "world.hpp"

// Bir step: ölçümü oku -> hesapla -> manevrayı uygula -> komutu yaz.
// Kod fighter'ın içinde çalışıyor gibi düşünülür; yalnız fighter sürülür.
class Autopilot
{
public:
    Message step(const Message& state);

private:
    // Bir kez tanımlanır, her step içleri doldurulur.
    World world_;                     // fighter, bogey ve saat
    
    FlightCommand fighter_command_;   // fighter'a giden komut
    FlightCommand bogey_command_;     // bogey'e giden komut: hold, YAML başlangıcı aynen geri gider

    Maneuvers maneuvers_;             // bütün manevralar (maneuver.hpp)

    int last_print_s_ = -1;           // terminale en son yazılan simülasyon saniyesi
};
