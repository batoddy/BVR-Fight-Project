#pragma once

#include <optional>
#include <string>
#include <vector>

#include "model/missile.hpp"
#include "model/missile_profiles.hpp"

// WEZ (Weapon Engagement Zone): bir füzenin bir hedefe karşı etkin menzil aralığı.
struct Wez
{
    double min_nm = 0;   // NM, iç sınır: daha yakında füze güvenle kullanılamaz
    double max_nm = 0;   // NM, dış sınır: düşman rotasını sürdürürse azami menzil; düşman bana geliyorsa uzar
};

// Uçağın silahları: füze slotları, atış emri ve füzemin atanmış düşmana karşı durumu.
//   own.weapons.fire(own.profile.main_weapon), own.weapons.enemy_in_wez, own.weapons.active_missiles() ...
// Kaçış tarafı (düşmanın füzesine karşı R_cold, NEZ, MAR, tehdit) own.picture'dadır.
struct Weapons
{
    std::vector<Missile> missiles;     // slot sırasıyla, boyutu sabit | StateReader (ilk step kurar: sayı Python'dan, tip loadout'tan)

    bool has_launch_order = false;     // bu step atış emri var mı | fire; her step otopilottan önce false (Sim::step)

    // Ana silahımın (profile.main_weapon) düşmana karşı durumu | World::update
    Wez wez;                           // benim WEZ'im
    std::optional<double> a_pole_nm;   // NM, A-pole; çözüm yoksa yok
    std::optional<double> f_pole_nm;   // NM, F-pole; çözüm yoksa yok
    std::optional<double> tr_nm;       // NM, TR; çözüm yoksa yok
    std::optional<double> msr_nm;      // NM, MSR; çözüm yoksa yok
    bool enemy_in_wez = false;         // düşman benim WEZ'imde mi
    bool enemy_in_nez = false;         // düşman benim NEZ'imde mi: wez.min_nm < menzil ≤ düşmanın picture.nez_nm'i
    bool enemy_in_tr = false;          // menzil ≤ TR mi; TR yoksa false
    bool enemy_in_msr = false;         // menzil ≤ MSR mi; MSR yoksa false
    double r_cold_nm = 0;              // NM, düşman benden kaçarken füzemin yetişebileceği en büyük mesafe (düşmanın picture.r_cold_nm'i)
    // TODO: m_pole_nm

    bool fire(const MissileProfile& profile);               // bu profilin ilk atılmamış slotu; füze yoksa ya da önceki emir henüz uçmadıysa false
    int missiles_left() const;                              // adet, atılmamış füze
    bool launch_pending() const;                            // atış emri verildi, Python henüz uçurmadı. Yalnız own için: düşmanın emri gerçekte bilinmez
    std::vector<const Missile*> active_missiles() const;    // havadaki güdümlü füzelerim, atış sırasıyla; yoksa boş. Bu step içinde kullan
    const Missile* active_missile() const;                  // en son atılan aktif (desteklediğim) füze; yoksa nullptr
};
