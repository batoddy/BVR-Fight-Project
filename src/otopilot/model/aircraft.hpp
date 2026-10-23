#pragma once

// Uçak verisi. fighter ve hostile aynı Aircraft sınıfını kullanır.
// JSON bilmez; alanları her step StateReader doldurur.
//
// Yorum düzeni:   birim | JSON anahtarı | Python / JSBSim kaynağı

#include <string>
#include <vector>
#include "model/aircraft_profiles.hpp"
#include "model/missile_profiles.hpp"
#include "common/util_functions.hpp"   // Ned

// Konum ve hareket. İleride atılmış füzede de aynı yapı kullanılacak.
struct Kinematics
{
    double lat_deg = 0;           // derece, WGS84 jeodezik enlem | lat_deg   | position/lat-geod-deg
    double lon_deg = 0;           // derece, boylam               | lon_deg   | position/long-gc-deg
    double altitude_ft = 0;       // ft | Python alt_msl_m: StateReader m → ft dönüştürür
    double heading_deg_180 = 0;   // derece ±180, 0 kuzey, + doğu | yaw_deg (0..360, okurken çevrilir) | attitude/psi-deg
    double pitch_deg = 0;         // derece, + burun yukarı       | pitch_deg | attitude/theta-deg
    double roll_deg = 0;          // derece, + sağ kanat aşağı    | roll_deg  | attitude/phi-deg
    double tas_kt = 0;            // KTAS, gerçek hava hızı | Python tas_mps: m/s → kt
    double mach = 0;              // birimsiz                     | mach      | velocities/mach
    
    // kt, [kuzey, doğu, aşağı] | Python velocity_ned_mps: üç bileşen de m/s → kt
    Ned velocity_ned_kt;
};


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
    None,   // henüz sonuç yok (atılmadı ya da havada)   | outcome null
    Hit,    // hedefi vurdu                               | "hit"
    Lost,   // ıskaladı / enerjisi bitti                  | "lost"
};


// Atılmış füzenin hedefine göre bilgileri. Ölçülmez, hesaplanır: World::update (MissileGeometry) doldurur.
// Yalnız füze havadayken güncellenir; füze bitince son değerler kalır.
//   world_.fighter.last_fired()->target.pitbull   füze kendi radarını açtı mı
struct MissileTargetInfo
{
    double range_nm = 0;        // NM, füze ile hedef arası 3B mesafe                       | MissileGeometry::range_nm
    double closure_kt = 0;    // kt, füze ile hedef arası mesafenin azalma hızı; + yaklaşıyor | MissileGeometry::closure_kt
    double time_to_go_s = 0;   // s, bu kapanma hızıyla hedefe kalan süre; uzaklaşıyorsa -1    | MissileGeometry::time_to_go_s
    bool pitbull = false;      // füze kendi radarıyla hedefi görüyor mu; bir kez true olunca kalır | MissileGeometry::pitbull
};


// Uçağın füze slotlarından biri. Liste ilk step'te Python'daki füze sayısı kadar kurulur,
// tipler uçak profilindeki loadout'tan gelir; liste hiç büyümez, küçülmez.
//   world_.fighter.missiles[i].profile.type        Fox1 / Fox2 / Fox3
//   world_.fighter.missiles[i].profile.max_range_nm menzil, .speed_kt hız ...
//   world_.fighter.missiles[i].fired               atıldı mı
//   world_.fighter.missiles[i].hit()               vurdu mu
// Kinematik ve durum ancak Python füzeyi uçurduktan sonra (has_kinematics) anlamlıdır.
struct Missile
{
    int slot = 0;                 // 0, 1, 2 ...: uçaktaki yeri; bizim kimliğimiz
    MissileProfile profile;       // tipi, menzili, hızı (missile_profiles.hpp; loadout'tan)

    bool fired = false;           // atış emri verildi mi (Aircraft::fire_missile)
    int fire_order = 0;           // kaçıncı atış: 1, 2, ...; atılmadıysa 0

    // Python'dan: aşağıdakiler ancak has_kinematics true olunca anlamlı.
    std::string python_id;        // "blue/missile/0"; eşleşene kadar boş   | id
    std::string target_id;        // hedef uçağın id'si; atılmadan önce boş | target_id
    bool has_kinematics = false;  // eşleşip ilk ölçüm gelince true
    MissileState state = MissileState::Ready;          // Ready / InFlight / Ended | state
    MissileOutcome outcome = MissileOutcome::None;     // vurdu mu                 | outcome
    Kinematics kinematics;        // füzenin konumu                         | kinematics

    MissileTargetInfo target;     // hesaplanan: hedefe göre (World::update)

    double f_pole_nm = 0.0;

    bool in_flight() const { return state == MissileState::InFlight; }
    bool hit() const { return outcome == MissileOutcome::Hit; }
    bool lost() const { return outcome == MissileOutcome::Lost; }
};


// WEZ (Weapon Engagement Zone): bir füzenin bir hedefe karşı etkin menzil aralığı.
// Düşman bu aralıktaysa füze onu vurabilir.
struct Wez
{
    double min_nm = 0;   // NM, iç sınır: daha yakında füze güvenle kullanılamaz | profile.min_range_nm
    double max_nm = 0;   // NM, dış sınır: düşman rotasını sürdürürse azami menzil; düşman bana geliyorsa uzar
};


// Uçağın şu an yaptığı iş. Sayı değerleri sabit: CSV'ye ve görüntüleyiciye sayı olarak gider.
// Yeni aşama eklersen sona ekle ve görüntüleyicideki ada da ekle (web/app.js PHASE_NAMES).
enum class Phase
{
    Hold = 0,    // rotayı koru
    Fire = 1,    // füze atışı
    Out = 2,     // düşmana kuyruk dön, kaç
    In = 3,      // düşmana geri dön
    Crank = 4,   // düşmanı radar sınırında tutarak dön
    Notch = 5    // düşmana dik uç (henüz manevrası yok)
};


// Karşı uçağa göre bilgiler. Ölçülmez, hesaplanır: World::update doldurur.
//   fighter.enemy: bizim hostile'e bakışımız
//   hostile.enemy:   hostile'in bize bakışı
struct EnemyInfo
{
    // Geometry: saf geometri
    double range_2d_nm = 0;        // NM, yatay mesafe (irtifa farkı yok)          | Geometry::range_2d_nm
    double range_3d_nm = 0;        // NM, 3B düz çizgi mesafe (irtifa farkı dahil) | Geometry::range_3d_nm
    double bearing_deg = 0;       // derece ±180, kuzeye göre karşı uçağın yönü  | Geometry::bearing_deg
    double ata_deg = 0;           // derece ±180, burnum ile karşı uçak arası; + sağımda | Geometry::ATA_deg
    double aspect_deg = 0;        // derece ±180, karşı uçağın burnu ile bana bakışı; 0 hot, 180 cold | Geometry::aspect_angle_deg
    Ned los_nm;                    // NM, benden karşı uçağa vektör: kuzey, doğu, aşağı (uzunluğu range_3d_nm) | Geometry::los_nm
    Ned los_unit;                 // birimsiz, aynı vektörün yönü (uzunluğu 1)                             | Geometry::los_unit
    double rate_of_closure_kt = 0;       // kt, mesafenin azalma hızı (Vc); + yaklaşıyoruz, ikimizin hareketi   | Geometry::rate_of_closure_kt
    double enemy_closing_kt = 0; // kt, yalnız karşı uçağın bana doğru hızı; + bana geliyor (WEZ için) | Geometry::enemy_closing_kt

    // Engagement: füze ve taktik (profil + geometri)
    Wez my_wez;                    // benim WEZ'im: ilk füzem (missiles[0]) bu düşmana karşı      | Engagement::wez
    bool enemy_in_my_wez = false;  // düşman benim WEZ'imde mi: my_wez.min_nm < range < my_wez.max_nm | Engagement::in_wez
    bool me_in_enemy_wez = false;  // ben düşmanın WEZ'inde miyim: karşı uçağın enemy_in_my_wez'i | World::update
    double mar_nm = 0;              // NM, Minimum Abort Range                                     | Engagement::mar_nm
    double dor_nm = 0;              // NM, Desired Out Range                                       | Engagement::dor_nm
};



struct Aircraft
{
    std::string id;          // "blue" / "red"   | id     | scenario.yaml agents anahtarı
    std::string name;        // görünen ad       | name   | scenario.yaml name
    std::string team;        // "Blue" / "Red"   | team   | scenario.yaml team
    std::string type;        // profil adı       | type   | scenario.yaml type
    double health = 0;       // 0..1, 1 sağlam   | health | BVRGym agent.healthPoints

    AircraftProfile profile;        // sabit: type ile aircraft_profiles.hpp tablosundan
    Kinematics kinematics;          // ölçüm

    // Füzeler. İlk step'te StateReader kurar (sayı Python'dan, tipler profile.loadout'tan),
    // sonraki step'lerde uçanları doldurur; fire_missile atışta işaretler.
    std::vector<Missile> missiles;      // slot sırasıyla; boyutu sabit
    bool launch_order = false;          // bu step atış emri var mı; her step false başlar | CommandWriter launch

    Phase phase = Phase::Hold;      // şu an hangi aşamada; otopilot her step yazar | calculations phase

    EnemyInfo enemy;                // hesaplanan: karşı uçağa göre

    double get_turn_radius_nm() const;
    double get_in_distance_nm(const Aircraft& enemy) const;

    double get_out_distance_nm(const Aircraft& enemy) const;
    // Füze at: o tipteki (FOX1 / FOX2 / FOX3) ilk atılmamış slot. Bu step launch emri gider.
    // O tipte füze kalmadıysa ya da bu step emri verilmiş bir füze varsa false döner; havadaki füze engel değil.
    bool fire_missile(const MissileProfile& type);

    // adet: atılmamış füze sayısı
    int missiles_left() const;

    // Bu uçağın belirtilen hedefe doğru hâlâ uçan bir füzesi var mı?
    bool has_active_missile_against(const Aircraft& target) const;

    // Son atılan füze; hiç atılmadıysa nullptr. İşaretçiyi bu step içinde kullan, saklama.
    const Missile* last_fired() const;
};
