#pragma once

// Python'dan gelen ölçümü ve Python'a giden komutu tutan nesneler.
// Yönler (yaw_deg, heading_deg) C++ içinde ±180'dir: 0 kuzey, + sağ, - sol.
// 0..360 <-> ±180 çevirisi yalnızca objects.cpp içinde yapılır.

#include <array>
#include <optional>
#include <string>
#include <vector>
#include "messages.hpp"
#include "aircraft_profiles.hpp"

// ---------------------------------------------------------------- ölçüm

// Konum ve hareket. Uçakta da füzede de aynı alanlar var.
struct Kinematics
{
    double lat_deg = 0;     // jeodezik enlem (WGS84), derece
    double lon_deg = 0;     // boylam, derece
    double alt_msl_m = 0;   // deniz seviyesine göre irtifa, m
    double yaw_deg = 0;     // burun yönü, ±180
    double pitch_deg = 0;
    double roll_deg = 0;
    double tas_mps = 0;     // gerçek hava hızı, m/s
    double mach = 0;
    std::array<double, 3> velocity_ned_mps{};  // kuzey, doğu, aşağı; m/s

    static Kinematics from_json(const Json& j);
};

// Uçak: simülasyondan gelen ölçüm + lookup table'dan gelen tip özellikleri.
struct Aircraft
{
    std::string id;           // "blue" / "red"
    std::string name;         // "KE"
    std::string type;         // "F16" -> profil bu adla bulunur
    double health = 0;        // 1 sağlam, isabet alınca düşer
    Kinematics kinematics;
    AircraftProfile profile;  // aircraft_profiles.hpp

    static Aircraft from_json(const Json& j);
};

struct Missile
{
    std::string id;           // "blue/missile/0"
    std::string owner_id;     // "blue"
    std::string target_id;    // atılmadıysa boş
    std::string state;        // "ready" / "in_flight" / "ended"
    std::string outcome;      // "hit" / "lost" / boş
    std::optional<double> flight_time_s;   // atılmadıysa yok
    std::optional<Kinematics> kinematics;  // atılmadıysa yok: if (m.kinematics) ...

    static Missile from_json(const Json& j);
};

// Bir turdaki bütün dünya. blue her zaman fighter, red her zaman bogey.
struct World
{
    double sim_time_s = 0;
    Aircraft fighter;
    Aircraft bogey;
    std::vector<Missile> fighter_missiles;
    std::vector<Missile> bogey_missiles;

    static World from_json(const Json& data);
};

// ---------------------------------------------------------------- komut

// Uçuşa ve atışa dair komut. heading_deg ±180 yazılır; gönderirken 0..360'a çevrilir.
// Yön/irtifa/gaz kalıcıdır: değiştirene kadar her tur aynısı gider.
// Atış tek seferliktir: gönderildikten sonra controller fire_target_id'yi temizler.
struct FlightCommand
{
    std::string id;               // "blue" / "red"
    double heading_deg = 0;       // ±180: + sağ, - sol
    double altitude_m = 0;
    double throttle = 0;          // 0..1
    std::string fire_target_id;   // boşsa bu tur atış yok

    // Bu tur hedefe ateş et. Mümkün değilse (havada kendi füzesi var / füze bitti) Python atmaz.
    void fire(const Aircraft& target)
    {
        fire_target_id = target.id;
    }

    static FlightCommand from_json(const Json& j);
    Json to_json() const;
};
