#pragma once

#include <map>
#include <stdexcept>
#include <string>
#include <vector>

#include "model/missile_profiles.hpp"

// Uçak tipinin sabit özellikleri. Simülasyondan gelmez; burada elle doldurulur.
// Yeni özellik: alanı struct'a, değerini tabloya aynı sırada yaz.
struct AircraftProfile
{
    std::string name;

    // Ana silah: WEZ, NEZ, F-pole ve düşmanın bana karşı R_cold/MAR hesabı bununla yapılır (yükten bağımsız).
    MissileProfile main_weapon;

    // Füze yükü: slot sırasıyla tipler. Python yalnız füze SAYISINI verir; tipleri buradan gelir.
    // Python'daki sayı (scenario.yaml ammo) kadar slot bu listenin başından alınır.
    std::vector<MissileProfile> loadout;

    double max_mach = 0;   // birimsiz; TODO: henüz kullanılmıyor
    double max_g = 0;      // g, yapısal sınır: komuttaki g_limit bunu aşamaz (FlightCommand::set_g_limit)

    // WEZ burada değil: main_weapon'ın menzili missile_profiles.hpp'de, WEZ engagement::wez_nm'de hesaplanır.

    double radar_range_nm = 0;     // NM, algılama menzili
    double radar_gimbal_deg = 0;   // derece, yarım açı: burundan sağa VE sola tarama açısı (60 = ±60)

    double out_g = 0.0;                    // g, out dönüşünde kullanılan yük faktörü (out planı ve Out manevrası)
    double out_offset_nm = 0;              // NM, MAR'a eklenen out payı
    double dr_notch_time_s = 15.0;         // s, DR hesabında notch bekleme süresi
    double recommit_fire_ata_deg = 30.0;   // derece, recommit'te atışın yapıldığı ATA; MRR hesabı bununla yapılır
};

// Değerlerin kaynağı:
//   F16  max_mach, max_g, radar: açık kaynak (F-16C Block 50/52, AN/APG-68).
//   Su57, CustomAircraft: tamamen örnek. Fizik modeli her tipte F-16.
//   Out g'si ve payı: yaklaşık simülasyon ayarları; ölçülmüş performans verisi değil. Menziller NM'dir.
//
/// Tip → profil tablosu. Anahtar: Python'dan gelen type (scenario.yaml). Değerler struct'taki sırayla.
inline const std::map<std::string, AircraftProfile>& aircraft_profiles()
{
    static const std::map<std::string, AircraftProfile> table = {
        //                   name               main_weapon  loadout (slot sırasıyla)                    mach   g     radar  gimbal  out_g  out_offset_nm
        {"F16",            {"F-16",             AIM120C,     {AIM120C, AIM120C, AIM120C, AIM120C},       2.05,  9.0,  200,    60,     6.0,   0.5}},
        {"Su57",           {"Su-57",            AIM120C,     {AIM120C, AIM120C, AIM120C, AIM9X, AIM9X},  2.0,   9.0,  200,    60,     6.0,   0.5}},
        {"CustomAircraft", {"Ornek ozel ucak",  AIM7M,       {AIM7M, AIM7M, AIM9X, AIM9X},               1.2,   4.0,  200,    45,     6.0,   0.5}},
    };
    return table;
}

/// Tipin profilini döndür. Tabloda yoksa anlaşılır hatayla dur.
inline const AircraftProfile& aircraft_profile(const std::string& type)
{
    const auto& table = aircraft_profiles();

    auto found = table.find(type);
    if (found == table.end())
    {
        throw std::runtime_error("Bilinmeyen ucak tipi: " + type + " (aircraft_profiles.hpp'ye ekle)");
    }

    return found->second;
}
