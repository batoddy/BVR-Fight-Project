#pragma once

#include <stdexcept>
#include <string>

// R_cold üstel uyumu: kaçan hedefe karşı bu füzenin yetişebileceği en büyük mesafe.
//   R_cold = safety · exp(a + b·x + c·y + d·x·y)   NM
//   x = h − 10 (km), y = V_kaçış − 700 (kt)
// Katsayıların nasıl ölçüldüğü: olcumler/r_cold/r_cold_formulu.md
struct RColdFit
{
    double a = 0;        // birimsiz, 10 km ve 700 kt'de ln(R_cold)
    double b = 0;        // 1/km, irtifa katsayısı
    double c = 0;        // 1/kt, kaçış hızı katsayısı
    double d = 0;        // 1/(km·kt), irtifa–hız etkileşimi
    double safety = 1;   // birimsiz, güvenlik çarpanı
};

// AIM-120C, yeni sim füzesi (aim120c5, loftsuz), kaçan hedefe erişim taraması (4-10 km, 430-740 kt): olcumler/faz4_olcum/uyum_sonuc.txt. Eski BVRGym: {2.646, 0.1523, -0.00201, -0.00008}
inline constexpr RColdFit AIM120C_R_COLD = {2.514, 0.1109, -0.00158, 0.00002, 1.1};

// Rmax uyumu: bu füzenin azami kinematik menzili (WEZ dış sınırı; BMS'te Raero). Füze hedefe enerjisi bitmeden erişir; isabet şart değil.
//   Rmax = R_m + V · t / 3600   NM
//   R_m = (a + b·ln h) · (1 + k·(V_atan − launch_ref_kt))   füzenin kendi gittiği yol, NM; h km
//   t   = c + d·ln h                                       uçuş süresi, s
//   V   = düşmanın bana doğru hızı, kt (+ geliyor, − kaçıyor)
// Ölçüm sonuçları ve uyum: olcumler/wez/wez_sonuc.txt
struct RmaxFit
{
    double a = 0;               // NM, R_m sabiti
    double b = 0;               // NM, R_m'nin ln(h) katsayısı
    double c = 0;               // s, t sabiti
    double d = 0;               // s, t'nin ln(h) katsayısı
    double k = 0;               // 1/kt, atan uçağın hızı katsayısı
    double launch_ref_kt = 0;   // kt, k'nın ölçüldüğü referans atış hızı
};

// AIM-120C, yeni sim füzesi (aim120c5, loftsuz), burun buruna erişim taraması (4-10 km, düşman bana doğru 430-740 kt, atan 430-740 kt): olcumler/faz4_olcum/uyum_sonuc.txt. Eski BVRGym: olcumler/wez/wez_sonuc.txt
inline constexpr RmaxFit AIM120C_R_MAX = {-2.314, 14.565, 13.59, 42.25, 0.000380, 583.0};

// Füze tipinin sabit özellikleri. Simülasyondan gelmez; burada elle doldurulur.
// Not: BVRGym her füzeyi AIM-120 fiziğiyle uçurur; tip yalnız C++ hesaplarında kullanılır.
struct MissileProfile
{
    std::string name;

    double min_range_nm = 0;       // NM, etkin menzil iç sınırı; WEZ dış sınırı r_max uyumundan
    double pitbull_range_nm = 0;   // NM, arayıcının kendi kilitlendiği füze-hedef mesafesi; kendi arayıcısı yoksa 0
    double blast_radius_ft = 0;    // ft, patlama yarıçapı
    RColdFit r_cold;               // bu füzeden kaçış hesabının uyumu
    RmaxFit r_max;                 // bu füzenin WEZ dış sınırının (Rmax) uyumu
    double slowing_accel_kt_s = 0;   // kt/s, uçuş uyumunun yavaşlaması; negatif
    double start_speed_kt = 0;       // kt, uçuş uyumunun başlangıç hızı (boost bitişi)
};

// Füzeler. Atış: own.weapons.fire()
//
// Değerler ÖRNEK. BVRGym hepsini AIM-120 fiziğiyle uçurduğu için R_cold, Rmax uyumu ve uçuş modeli hepsinde aynı.
//                                     name        min_nm  pitbull_nm  blast_ft  r_cold          r_max          accel_kt_s start_kt
inline const MissileProfile AIM7M   = {"AIM-7M",   1,      9,          10,       AIM120C_R_COLD, AIM120C_R_MAX, -18,       2700};
inline const MissileProfile AIM9X   = {"AIM-9X",   0.2,    0.0,        10,       AIM120C_R_COLD, AIM120C_R_MAX, -18,       2700};
inline const MissileProfile AIM120C = {"AIM-120C", 1,      10,         10,       AIM120C_R_COLD, AIM120C_R_MAX, -18,       2700};

inline const MissileProfile& missile_profile(const std::string& designation)   // hello'daki designation → profil; tabloda yoksa hata
{
    static const MissileProfile* const table[] = {&AIM7M, &AIM9X, &AIM120C};
    for (const MissileProfile* profile : table)
    {
        if (profile->name == designation)
        {
            return *profile;
        }
    }
    throw std::runtime_error("Bilinmeyen fuze: " + designation + " (missile_profiles.hpp'ye ekle)");
}
