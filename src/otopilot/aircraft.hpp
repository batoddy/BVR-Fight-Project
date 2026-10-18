#pragma once

// Uçak verisi. fighter ve bogey aynı Aircraft sınıfını kullanır.
// JSON bilmez; alanları her step StateReader doldurur.
//
// Yorum düzeni:   birim | JSON anahtarı | Python / JSBSim kaynağı

#include <array>
#include <string>
#include <vector>
#include "aircraft_profiles.hpp"


// Konum ve hareket. İleride atılmış füzede de aynı yapı kullanılacak.
struct Kinematics
{
    double lat_deg = 0;           // derece, WGS84 jeodezik enlem | lat_deg   | position/lat-geod-deg
    double lon_deg = 0;           // derece, boylam               | lon_deg   | position/long-gc-deg
    double altitude_m = 0;        // m, deniz seviyesinden (MSL)  | alt_msl_m | position/h-sl-meters
    double heading_deg_180 = 0;   // derece ±180, 0 kuzey, + doğu | yaw_deg (0..360, okurken çevrilir) | attitude/psi-deg
    double pitch_deg = 0;         // derece, + burun yukarı       | pitch_deg | attitude/theta-deg
    double roll_deg = 0;          // derece, + sağ kanat aşağı    | roll_deg  | attitude/phi-deg
    double tas_mps = 0;           // m/s, gerçek hava hızı        | tas_mps   | velocities/vt-fps x 0.3048
    double mach = 0;              // birimsiz                     | mach      | velocities/mach

    // m/s, [kuzey, doğu, aşağı] | velocity_ned_mps | velocities/v-north-fps, v-east-fps, v-down-fps x 0.3048
    std::array<double, 3> velocity_ned_mps{};
};


// Füzenin durumu. Sayı değerleri sabit: 0 atılmadı, 1 havada, 2 işi bitti.
enum class MissileState
{
    Ready = 0,      // atılmadı, rampada    | "ready"     | BVRGym is_ready_to_launch()
    InFlight = 1,   // havada               | "in_flight" | BVRGym is_active()
    Ended = 2,      // vurdu ya da kaybetti | "ended"     | sonuç outcome'da
};


// Uçağın füzelerinden biri. Sahibi, füzeyi missiles listesinde tutan Aircraft'tır.
struct Missile
{
    std::string id;          // "blue/missile/0"                 | id
    std::string target_id;   // hedef uçak id; atılmadıysa boş   | target_id (null olabilir)
    MissileState state = MissileState::Ready;   //               | state (metin, okurken çevrilir)
    std::string outcome;     // "hit" / "lost"; sonuç yoksa boş  | outcome (null olabilir)

    // Ready iken aşağıdaki iki alan anlamsız (0 kalır); okumadan önce state'e bak.
    double flight_time_s = 0;   // s, atıştan beri | flight_time_s
    Kinematics kinematics;      // füzenin konumu  | kinematics
};


// Kendi bilgilerim. Ölçülmez, hesaplanır: World::update doldurur. İhtiyaç oldukça alan eklenir.
struct OwnInfo
{
    int ready_missiles = 0;          // adet, atılmaya hazır füze
    int missile_in_flight = 0;      // havada füze sayısı
};


// Karşı uçağa göre bilgiler. Ölçülmez, hesaplanır: World::update doldurur.
//   fighter.enemy: bizim bogey'e bakışımız
//   bogey.enemy:   bogey'in bize bakışı
struct EnemyInfo
{
    // Geometry: saf geometri
    double range_2d_m = 0;        // m, yatay mesafe (irtifa farkı yok)          | Geometry::range_2d_m
    double range_3d_m = 0;        // m, 3B düz çizgi mesafe (irtifa farkı dahil) | Geometry::range_3d_m
    double bearing_deg = 0;       // derece ±180, kuzeye göre karşı uçağın yönü  | Geometry::bearing_deg
    double ata_deg = 0;           // derece ±180, burnum ile karşı uçak arası; + sağımda | Geometry::ATA_deg

    // Weapons: füze ve taktik (profil + geometri)
    bool in_wez = false;          // karşı uçak benim WEZ'imde mi                 | Weapons::in_wez
    double mar_m = 0;             // m, Minimum Abort Range                       | Weapons::mar_m
    double dor_m = 0;             // m, Desired Out Range                         | Weapons::dor_m
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
    std::vector<Missile> missiles;  // ölçüm: kendi füzeleri | data.missiles, owner_id == id

    OwnInfo own;                    // hesaplanan: kendi bilgilerim
    EnemyInfo enemy;                // hesaplanan: karşı uçağa göre

};
