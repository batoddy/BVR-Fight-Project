#pragma once

#include <string>

// Füze rehberlik tipi: atıştan sonra ne kadar destek gerektiğini belirler.
enum class MissileType
{
    Fox1,   // yarı aktif radar: atan uçağın radarı isabete kadar aydınlatmalı
    Fox2,   // kızılötesi: destek gerekmez, kısa menzil
    Fox3,   // aktif radar: pitbull'a (arayıcı aktif) kadar destek, sonra serbest
};

// Füze tipinin sabit özellikleri. Simülasyondan gelmez; burada elle doldurulur.
// Not: BVRGym her füzeyi AIM-120 fiziğiyle uçurur; tip yalnız C++ hesaplarında kullanılır.
struct MissileProfile
{
    std::string name;
    MissileType type = MissileType::Fox3;

    double max_range_nm = 0;      // NM, etkin menzil dış sınırı
    double min_range_nm = 0;      // NM, etkin menzil iç sınırı
    double speed_kt = 0;        // kt, ortalama hız (uçuş süresi tahmini için)
    double pitbull_range_nm = 0;  // NM, Fox3: arayıcının kendi kilitlendiği füze-hedef mesafesi; diğerlerinde 0
    double blast_radius_ft = 0;
};

// Üç füze. Koşulda doğrudan kullanılır:  world_.fighter.enemy.range_3d_nm < FOX3.a_pole_nm
// Atış:                                   world_.fighter.fire_missile(FOX3)
//
// Değerler ÖRNEK. AIM-120C menzili BVRGym'de ölçülen değer (kafa kafaya, 8000 m, 300 m/s).
//                                  name        type               max_nm   min_nm  speed_kt  pitbull_nm  blast_radius_ft
inline const MissileProfile FOX1 = {"AIM-7M", MissileType::Fox1,   14, 1,   1750, 0.0, 10};
inline const MissileProfile FOX2 = {"AIM-9X", MissileType::Fox2,   6,  0.2, 1550, 0.0, 10};
inline const MissileProfile FOX3 = {"AIM-120C", MissileType::Fox3, 27, 1,   1940, 8,   10}; 
