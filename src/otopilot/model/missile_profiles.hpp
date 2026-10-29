#pragma once

#include <string>

// Füze rehberlik tipi: atıştan sonra ne kadar destek gerektiğini belirler.
enum class MissileType
{
    Fox1,   // yarı aktif radar: atan uçağın radarı isabete kadar aydınlatmalı
    Fox2,   // kızılötesi: destek gerekmez, kısa menzil
    Fox3,   // aktif radar: pitbull'a (arayıcı aktif) kadar destek, sonra serbest
};

// Füzenin yavaşlama (süzülme) eğrisinin bir satırı: bu Mach'a düştüğünde ne kadar yol aldı, ne kadar süre geçti.
struct MissileGlidePoint
{
    double mach;         // birimsiz
    double distance_m;   // m, atıştan beri uçulan yol
    double time_s;       // s, atıştan beri geçen süre
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

    // Yavaşlama eğrisi: Mach artan sırada, EŞİT ADIMLI (indeks Mach'tan doğrudan hesaplanır).
    // Tablo tek bir koşulda ölçülmüştür (AIM-120C: 10 km, ses hızı 299.5 m/s).
    // Boşsa bu füze için eğri yok.
    std::vector<MissileGlidePoint> glide_table_10km;
};
// AIM-120C, 10 km, 45 kg yakıt, 7 s yanma. Mach-süre ve Mach-mesafe grafiklerinden
// 0.1 Mach adımlarla ara değer alınarak türetildi. Yalnız tepe hızdan (3.3) sonrası.
inline const std::vector<MissileGlidePoint> AIM120C_GLIDE_TABLE_10KM = {
    //  Mach    m        s
    { 0.8,  51500,  100.0 },   // eğrinin sonu
    { 0.9,  49143,   92.3 },
    { 1.0,  47000,   85.0 },
    { 1.1,  45000,   78.1 },
    { 1.2,  43000,   71.9 },
    { 1.3,  41000,   66.3 },
    { 1.4,  39038,   61.1 },
    { 1.5,  37115,   56.5 },
    { 1.6,  35192,   52.2 },
    { 1.7,  33269,   48.1 },
    { 1.8,  31346,   44.4 },
    { 1.9,  29455,   40.7 },
    { 2.0,  27636,   37.7 },
    { 2.1,  25818,   34.9 },
    { 2.2,  24000,   32.0 },
    { 2.3,  22182,   29.2 },
    { 2.4,  20364,   26.6 },
    { 2.5,  18545,   23.9 },
    { 2.6,  16727,   21.3 },
    { 2.7,  14909,   18.9 },
    { 2.8,  13091,   16.8 },
    { 2.9,  11273,   14.7 },
    { 3.0,   9455,   12.6 },
    { 3.1,   7636,   10.4 },
    { 3.2,   5818,    8.7 },
    { 3.3,   4000,    7.0 },   // tepe hız, yanma bitti
};



// Üç füze. Koşulda doğrudan kullanılır:  world_.fighter.enemy.range_3d_nm < FOX3.a_pole_nm
// Atış:                                   world_.fighter.fire_missile(FOX3)
//
// Değerler ÖRNEK. AIM-120C menzili BVRGym'de ölçülen değer (kafa kafaya, 8000 m, 300 m/s).
//                                  name        type               max_nm   min_nm  speed_kt  pitbull_nm  blast_radius_ft
inline const MissileProfile FOX1 = {"AIM-7M", MissileType::Fox1,   14, 1,   1750, 0.0, 10, AIM120C_GLIDE_TABLE_10KM};
inline const MissileProfile FOX2 = {"AIM-9X", MissileType::Fox2,   6,  0.2, 1550, 0.0, 10, AIM120C_GLIDE_TABLE_10KM};
//                                  name        type               max_nm   min_nm  speed_kt  pitbull_nm  blast_radius_ft  mach_by_time           mach_by_distance
inline const MissileProfile FOX3 = {"AIM-120C", MissileType::Fox3, 27, 1,   1940, 8,   10, AIM120C_GLIDE_TABLE_10KM};
