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
    Ended = 2,      // vurdu ya da kaybetti | "ended"     | sonuç outcome'da
};

// Füzenin sonucu: vurdu mu, kaybetti mi.
enum class MissileOutcome
{
    None,   // henüz sonuç yok (atılmadı ya da havada) | outcome null
    Hit,    // hedefi vurdu                             | "hit"
    Lost,   // ıskaladı / enerjisi bitti                | "lost"
};

// Atılmış füzenin kendi hedefine göre bilgileri | World::update (missile_geometry)
// Yalnız füze havadayken güncellenir; füze bitince son değerler kalır.
struct MissileTargetInfo
{
    double range_nm = 0;                   // NM, füze ile hedef arası 3B mesafe
    double closure_kt = 0;                 // kt, füze ile hedef arası mesafenin azalma hızı; + yaklaşıyor
    bool is_pitbull = false;               // füze kendi radarıyla hedefi görüyor mu; bir kez true olunca kalır
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

    // Python'dan: aşağıdakiler ancak has_kinematics true olunca anlamlı.        | StateReader
    std::string python_id;         // "blue/missile/0"; eşleşene kadar boş          | id
    std::string target_id;         // hedef uçağın id'si; atılmadan önce boş        | target_id
    bool has_kinematics = false;   // eşleşip ilk ölçüm gelince true
    MissileState state = MissileState::Ready;        // | state
    MissileOutcome outcome = MissileOutcome::None;   // | outcome
    Kinematics kinematics;         // füzenin konumu                               | kinematics

    MissileTargetInfo target;      // hedefe göre                                  | World::update

    /// Havada mı (state InFlight)
    bool in_flight() const { return state == MissileState::InFlight; }
    /// Hedefi vurdu mu
    bool hit() const { return outcome == MissileOutcome::Hit; }
    /// Iskaladı ya da enerjisi bitti mi
    bool lost() const { return outcome == MissileOutcome::Lost; }
};
