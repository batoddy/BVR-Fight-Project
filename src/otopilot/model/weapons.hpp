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
//   own.weapons.fire(AIM120C), own.weapons.enemy_in_nez, own.weapons.active_missile() ...
// Kaçış tarafı (düşmanın füzesine karşı R_cold, NEZ, MAR, tehdit) own.picture'dadır.
struct Weapons
{
    // Füze slotları, slot sırasıyla; boyutu sabit.
    // İlk step'te StateReader kurar (sayı Python'dan, tipler profile.loadout'tan), sonra uçanları doldurur.
    std::vector<Missile> missiles;

    bool has_launch_order = false;   // bu step atış emri var mı; her step false başlar (StateReader) | fire

    // Ana silahımın (profile.main_weapon) düşmana karşı durumu | World::update
    Wez wez;                           // benim WEZ'im
    double nez_nm = 0;                 // NM, benim NEZ'im: düşman şimdi out yapsa füzemden kaçamayacağı mesafe
    std::optional<double> f_pole_nm;   // NM, füzem vurduğunda aramızda kalacak mesafe; füze yetişemiyorsa yok
    bool enemy_in_wez = false;         // düşman benim WEZ'imde mi
    bool enemy_in_nez = false;         // düşman benim NEZ'imde mi: wez.min_nm < menzil <= nez_nm
    // TODO: m_pole_nm, a_pole_nm

    // Füze at: bu profildeki (ör. AIM120C) ilk atılmamış slot. Bu step launch emri gider.
    // O profilde füze kalmadıysa ya da emri verilmiş ama henüz uçmamış füze varsa false; havadaki füze engel değil.
    bool fire(const MissileProfile& profile);

    // adet: atılmamış füze sayısı
    int missiles_left() const;

    // target_id'ye doğru hâlâ uçan bir füzem var mı
    bool has_active_missile_against(const std::string& target_id) const;

    // Son atılan füze; hiç atılmadıysa nullptr. İşaretçiyi bu step içinde kullan, saklama.
    const Missile* last_fired() const;

    // Havadaki füzelerimden en son atılanı (desteklediğim füze); yoksa nullptr. Bu step içinde kullan.
    const Missile* active_missile() const;
};
