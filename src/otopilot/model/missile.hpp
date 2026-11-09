#pragma once

#include <optional>
#include <string>

#include "model/kinematics.hpp"
#include "model/missile_profiles.hpp"

// Füzenin durumu. Sayı değerleri sabit: 0 atılmadı, 1 havada, 2 işi bitti.
enum class MissileState
{
    Ready = 0,      // atılmadı, rampada    | "ready"     | BVRGym is_ready_to_launch()
    InFlight = 1,   // havada               | "in_flight" | BVRGym is_active()
    Ended = 2,      // vurdu ya da kaybetti | "ended"
};

// Atılmış füzenin kendi hedefine göre bilgileri | World::update (missile_geometry)
// Yalnız füze havadayken güncellenir; füze bitince son değerler kalır.
struct MissileTargetInfo
{
    double range_nm = 0;                   // NM, füze ile hedef arası 3B mesafe
    double closure_kt = 0;                 // kt, füze ile hedef arası mesafenin azalma hızı; + yaklaşıyor
    bool is_pitbull = false;               // füze kendi radarıyla hedefi görüyor mu; bir kez true olunca kalır
    std::optional<double> a_pole_nm;       // NM, atış anındaki weapons.a_pole_nm (Weapons::fire); uçuşta sabit
    std::optional<double> f_pole_nm;       // NM, atış anındaki weapons.f_pole_nm (Weapons::fire); uçuşta sabit
    std::optional<double> catch_time_left_s;   // s, füzenin hedefe yetişmesine kalan süre (missile_geometry::catch_time_left_s); yetişemiyorsa yok
    std::optional<double> pitbull_time_left_s; // s, pitbull'a kalan süre (missile_geometry::pitbull_time_left_s); pitbull ise 0, erişemiyorsa yok
    bool is_guided = true;                 // pitbull ya da atan uçak destekliyor (missile_geometry::is_guided); bir kez false olunca kalır
};

// Uçağın füze slotlarından biri. Liste ilk step'te Python'daki füze sayısı kadar kurulur,
// tipler uçak profilindeki loadout'tan gelir; liste hiç büyümez, küçülmez.
// Kinematik ve durum ancak Python füzeyi uçurduktan sonra (has_kinematics) anlamlıdır.
struct Missile
{
    int slot = 0;                  // 0, 1, 2 ...: uçaktaki yeri; bizim kimliğimiz | StateReader
    MissileProfile profile;        // tipi, menzili, hızı (loadout'tan)            | StateReader

    bool is_fired = false;         // atış emri verildi mi                         | Weapons::fire
    int fire_order = 0;            // kaçıncı atış: 1, 2, ...; atılmadıysa 0       | Weapons::fire

    // Python'dan; ancak has_kinematics true olunca anlamlı                       | StateReader
    std::string python_id;         // "blue/missile/0"; eşleşene kadar boş          | id
    std::string target_id;         // hedef uçağın id'si; atılmadan önce boş        | target_id
    bool has_kinematics = false;   // eşleşip ilk ölçüm gelince true
    MissileState state = MissileState::Ready;        // | state
    Kinematics kinematics;         // füzenin konumu                               | kinematics

    MissileTargetInfo target;      // hedefe göre                                  | World::update

    bool in_flight() const { return state == MissileState::InFlight; }   // havada mı
};
