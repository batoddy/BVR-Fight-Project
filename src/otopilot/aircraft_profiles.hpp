#pragma once

#include <map>
#include <stdexcept>
#include <string>

// Uçak tipinin sabit özellikleri. Simülasyondan gelmez; burada elle doldurulur.
// Yeni özellik: alanı struct'a, değerini tabloya aynı sırada yaz.
struct AircraftProfile
{
    std::string name;

    double max_mach = 0;               // birimsiz
    double max_g = 0;                  // g

    double wez_max_range_m = 0;        // m, füzenin etkin menzil dış sınırı
    double wez_min_range_m = 0;        // m, füzenin etkin menzil iç sınırı

    double radar_range_m = 0;          // m, algılama menzili
    double radar_gimbal_half_deg = 0;  // derece, burundan sağa VE sola tarama açısı (60 = ±60)
};

// Değerlerin kaynağı:
//   F16  max_mach, max_g, radar: açık kaynak (F-16C Block 50/52, AN/APG-68).
//        wez: gerçek AIM-120 DEĞİL; BVRGym füze modelinde kafa kafaya, 8000 m, 300 m/s ölçüldü.
//   Su57, CustomAircraft: tamamen örnek. Fizik modeli her tipte F-16.
//
// Anahtar: Python'dan gelen type (scenario.yaml). Değerler struct'taki sırayla.
inline const std::map<std::string, AircraftProfile>& aircraft_profiles()
{
    static const std::map<std::string, AircraftProfile> table = {
        //                   name               mach   g     wez_max  wez_min  radar   gimbal
        {"F16",            {"F-16",             2.05,  9.0,  31000,   2000,    80000,  60}},
        {"Su57",           {"Su-57",            2.0,   9.0,  31000,   2000,   100000,  60}},
        {"CustomAircraft", {"Ornek ozel ucak",  1.2,   4.0,  20000,   3000,    50000,  45}},
    };
    return table;
}

// Tipin profilini döndür. Tabloda yoksa anlaşılır hatayla dur.
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
