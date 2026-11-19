#pragma once

#include <optional>
#include <string>

#include "model/kinematics.hpp"
#include "model/profiles/missile_profiles.hpp"

// Füzenin durumu. Sayı değerleri sabit: 0 atılmadı, 1 havada, 2 işi bitti.
enum class MissileState
{
    Ready = 0,      // atılmadı, rampada    | "ready"     | BVRGym is_ready_to_launch()
    InFlight = 1,   // havada               | "in_flight" | BVRGym is_active()
    Ended = 2,      // vurdu ya da kaybetti | "ended"
};

// Füzenin güdüm kipi (sim'in guidance_mode'u).
enum class GuidanceMode
{
    None,       // güdüm yok ya da atılmadı | "none"
    Datalink,   // atan uçağın güncel izi   | "datalink"
    Inertial,   // son izden tahmin         | "inertial"
    Seeker,     // kendi arayıcısı          | "seeker"
};

enum class MissileOutcome
{
    Hit,    // "hit"
    Lost,   // "lost"
};

// Atılmış füzenin kendi hedefine göre bilgileri | World::update (missile_geometry); is_pitbull ve is_guided sim'den (StateReader)
// Hesaplananlar yalnız füze havadayken güncellenir; füze bitince son değerler kalır.
struct MissileTargetInfo
{
    double range_nm = 0;                   // NM, füze ile hedef arası 3B mesafe
    double closure_kt = 0;                 // kt, füze ile hedef arası mesafenin azalma hızı; + yaklaşıyor
    bool is_pitbull = false;               // füze kendi radarıyla hedefi gördü mü; bir kez true olunca kalır | StateReader (sim is_pitbull)
    std::optional<double> a_pole_nm;       // NM, atış anındaki weapons.a_pole_nm (Weapons::fire); uçuşta sabit
    std::optional<double> f_pole_nm;       // NM, atış anındaki weapons.f_pole_nm (Weapons::fire); uçuşta sabit
    std::optional<double> catch_time_left_s;   // s, füzenin hedefe yetişmesine kalan süre (missile_geometry::catch_time_left_s); yetişemiyorsa yok
    std::optional<double> pitbull_time_left_s; // s, pitbull'a kalan süre (missile_geometry::pitbull_time_left_s); pitbull ise 0, erişemiyorsa yok
    bool is_guided = true;                 // füze şu an kilitli mi (datalink ya da seeker) | StateReader (sim is_locked)
};

// Uçağın füze slotlarından biri. Liste hello mesajındaki slotlardan kurulur (tip designation ile missile_profiles'tan);
// liste hiç büyümez, küçülmez. Sim'in slotuyla birebir; hangi slotu atacağını sim seçer.
// Kinematik ancak Python füzeyi uçurduktan sonra (has_kinematics) anlamlıdır.
struct Missile
{
    int slot = 0;                  // 0, 1, 2 ...: uçaktaki yeri; sim'deki slotla aynı | StateReader::read_hello
    MissileProfile profile;        // tipi, menzili, hızı (hello designation'dan)       | StateReader::read_hello

    int fire_order = 0;            // kaçıncı atış: 1, 2, ...; atılmadıysa 0            | Weapons::record_launch

    // Sim'den, her step                                                               | StateReader
    std::string target_id;         // hedef uçağın id'si; atılmadan önce boş             | target_id
    bool has_kinematics = false;   // sim füzeyi uçurunca true                           | kinematics null değil
    MissileState state = MissileState::Ready;          // | state
    GuidanceMode guidance_mode = GuidanceMode::None;   // | guidance_mode
    std::optional<MissileOutcome> outcome;             // bitince sonucu | outcome
    Kinematics kinematics;         // füzenin konumu                                    | kinematics

    MissileTargetInfo target;      // hedefe göre                                  | World::update

    bool in_flight() const { return state == MissileState::InFlight; }   // havada mı
};
