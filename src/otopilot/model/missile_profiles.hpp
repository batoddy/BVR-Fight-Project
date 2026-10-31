#pragma once

#include <string>

#include "model/missile_glide.hpp"

// Füze rehberlik tipi: atıştan sonra ne kadar destek gerektiğini belirler.
enum class MissileType
{
    Fox1,   // yarı aktif radar: atan uçağın radarı isabete kadar aydınlatmalı
    Fox2,   // kızılötesi: destek gerekmez, kısa menzil
    Fox3,   // aktif radar: pitbull'a (arayıcı aktif) kadar destek, sonra serbest
};

// R_cold üstel uyumu: kaçan hedefe karşı bu füzenin yetişebileceği en büyük mesafe.
//   R_cold = safety · exp(a + b·x + c·y + d·x·y)   NM
//   x = h − 10 (km), y = V_kaçış − 700 (kt)
// Katsayıların nasıl ölçüldüğü: calculations/r_cold_formulu.md
struct RColdFit
{
    double a = 0;        // birimsiz, 10 km ve 700 kt'de ln(R_cold)
    double b = 0;        // 1/km, irtifa katsayısı
    double c = 0;        // 1/kt, kaçış hızı katsayısı
    double d = 0;        // 1/(km·kt), irtifa–hız etkileşimi
    double safety = 1;   // birimsiz, güvenlik çarpanı
};

// AIM-120C, BVRGym füzesi, kaçan hedefe atış taraması (4-10 km, 570-1000 kt).
inline constexpr RColdFit AIM120C_R_COLD = {2.646, 0.1523, -0.00201, -0.00008, 1.1};

// Rmax uyumu: bu füzenin azami kinematik menzili (WEZ dış sınırı; BMS'te Raero). Füze hedefe enerjisi bitmeden erişir; isabet şart değil.
//   Rmax = R_m + V · t / 3600   NM
//   R_m = (a + b·ln h) · (1 + k·(V_atan − launch_ref_kt))   füzenin kendi gittiği yol, NM; h km
//   t   = c + d·ln h                                       uçuş süresi, s
//   V   = düşmanın bana doğru hızı, kt (+ geliyor, − kaçıyor)
// Katsayıların nasıl ölçüldüğü: calculations/wez_formulu.md
struct RmaxFit
{
    double a = 0;               // NM, R_m sabiti
    double b = 0;               // NM, R_m'nin ln(h) katsayısı
    double c = 0;               // s, t sabiti
    double d = 0;               // s, t'nin ln(h) katsayısı
    double k = 0;               // 1/kt, atan uçağın hızı katsayısı
    double launch_ref_kt = 0;   // kt, k'nın ölçüldüğü referans atış hızı
};

// AIM-120C, BVRGym füzesi, burun buruna erişim taraması (4-10 km, düşman bana doğru 420-1120 kt, atan 430-740 kt).
inline constexpr RmaxFit AIM120C_R_MAX ={-29.595, 39.193, -48.62, 95.66, 0.000417, 583.0};

// Füze tipinin sabit özellikleri. Simülasyondan gelmez; burada elle doldurulur.
// Not: BVRGym her füzeyi AIM-120 fiziğiyle uçurur; tip yalnız C++ hesaplarında kullanılır.
struct MissileProfile
{
    std::string name;
    MissileType type = MissileType::Fox3;

    double max_range_nm = 0;       // NM, açık kaynak menzili; hesaplarda kullanılmaz (WEZ dış sınırı r_max'tan)
    double min_range_nm = 0;       // NM, etkin menzil iç sınırı
    double speed_kt = 0;           // kt, ortalama hız (uçuş süresi tahmini için)
    double pitbull_range_nm = 0;   // NM, Fox3: arayıcının kendi kilitlendiği füze-hedef mesafesi; diğerlerinde 0
    double blast_radius_ft = 0;    // ft, patlama yarıçapı
    RColdFit r_cold;               // bu füzeden kaçış hesabının uyumu
    RmaxFit r_max;                 // bu füzenin WEZ dış sınırının (Rmax) uyumu
    GlideModel glide;             // ölçülmüş uçuş eğrisi: ne zaman nerede, hangi hızda (missile_flight::glide_...)
};

// Füzeler. Atış: own.weapons.fire(AIM120C)
//
// Değerler ÖRNEK. BVRGym hepsini AIM-120 fiziğiyle uçurduğu için R_cold, Rmax uyumu ve uçuş modeli hepsinde aynı.
//                                     name        type               max_nm  min_nm  speed_kt  pitbull_nm  blast_ft  r_cold          r_max          glide
inline const MissileProfile AIM7M   = {"AIM-7M",   MissileType::Fox1, 14,     1,      1750,     0.0,        10,       AIM120C_R_COLD, AIM120C_R_MAX, AIM120C_GLIDE};
inline const MissileProfile AIM9X   = {"AIM-9X",   MissileType::Fox2, 6,      0.2,    1550,     0.0,        10,       AIM120C_R_COLD, AIM120C_R_MAX, AIM120C_GLIDE};
inline const MissileProfile AIM120C = {"AIM-120C", MissileType::Fox3, 50,     1,      1940,     8,          10,       AIM120C_R_COLD, AIM120C_R_MAX, AIM120C_GLIDE};
