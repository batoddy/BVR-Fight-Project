#pragma once

#include <map>
#include <stdexcept>
#include <string>

// Uçak tipinin sabit özellikleri. Simülasyondan gelmez; burada elle doldurulur.
// Yeni özellik eklerken alanı buraya, değerini aşağıdaki tabloya aynı sırada yaz.
// 0 = henüz girilmedi.
struct AircraftProfile {
    std::string name;

    // Performans
    double max_mach = 0;
    double max_g = 0;

    // Silah: WEZ (silahın etkin olduğu menzil aralığı)
    double wez_max_range_m = 0;    // dış sınır
    double wez_min_range_m = 0;    // iç sınır

    // Radar
    double radar_range_m = 0;      // algılama menzili
    double radar_gimbal_deg = 0;   // burundan sağa/sola tarama yarı açısı

    // Manevra
    double turn_range_m = 0;       // dönüş / out için gereken menzil
};

// Değerlerin kaynağı (12 Ekim 2026):
//
// F16 — açık kaynak, F-16C Block 50/52:
//   max_mach 2.05, max_g 9   Wikipedia "F-16 Fighting Falcon" Specifications (40 000 ft, clean)
//   radar 80 km, ±60°        Wikipedia "AN/APG-68" (Deagel kaynaklı). Kaynaklar menzilde
//                            tutarsız (80 / 105 / 296 km); hedef boyutu belirtilmemiş.
//   wez                      Gerçek AIM-120 değeri DEĞİL. BVRGym'in AIM modelinde ölçüldü:
//                            kafa kafaya, iki uçak 8000 m, 300 m/s, hedef manevrasız ->
//                            31 km isabet, 34 km kayıp; 2 km isabet (daha yakını denenmedi).
//                            Geometriye bağlı: 3000 m'deki hedefe 28 km'den kayıp görüldü.
//   turn_range_m             ÖRNEK: taktik tercih, uçak verisi değil.
//
// Su57, CustomAircraft — TAMAMEN ÖRNEK, doğrulanmamış. Fizik modeli bu prototipte
// her tipte F-16 olduğu için WEZ değerleri F16 ile aynı bırakıldı.
//
// Not: JSBSim F-16 modeli ve BVRGym PID'si bu sınırları kendisi uygulamaz;
// bunlar senin hesabında kullanacağın referans değerlerdir.

// Anahtar: Python'dan gelen type adı. Değerler struct'taki sırayla yazılır.
inline const std::map<std::string, AircraftProfile>& aircraft_profiles() {
    static const std::map<std::string, AircraftProfile> table = {
        //                   name               max_mach  max_g  wez_max_m  wez_min_m  radar_m  gimbal_deg  turn_m
        {"F16",            {"F-16",              2.05,    9.0,     31000,     2000,    80000,       60,    40000}},
        {"Su57",           {"Su-57",             2.0,     9.0,     31000,     2000,   100000,       60,    40000}},
        {"CustomAircraft", {"Ornek ozel ucak",   1.2,     4.0,     20000,     3000,    50000,       45,    30000}},
    };
    return table;
}

// Tipin profilini döndür. Tablo bu tipi bilmiyorsa ilk ölçümde anlaşılır hatayla durur.
inline const AircraftProfile& aircraft_profile(const std::string& type) {
    const auto& table = aircraft_profiles();
    auto found = table.find(type);
    if (found == table.end())
        throw std::runtime_error("Bilinmeyen ucak tipi: " + type + " (aircraft_profiles.hpp'ye ekle)");
    return found->second;
}
