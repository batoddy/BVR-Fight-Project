#include "state_reader.hpp"
#include <stdexcept>
#include "util_functions.hpp"
#include "roles.hpp"

void StateReader::read_world(const Json& data, World& world)
{
    world.sim_time_s = data.at("sim_time_s").get<double>();   // s

    bool fighter_found = false;
    bool bogey_found = false;

    for (const Json& j : data.at("aircraft"))
    {
        std::string id = j.at("id").get<std::string>();
        if (id == FIGHTER_ID)
        {
            read_aircraft(j, world.fighter);
            fighter_found = true;
        }
        else if (id == BOGEY_ID)
        {
            read_aircraft(j, world.bogey);
            bogey_found = true;
        }
    }

    // Biri eksikse sessizce 0'larla devam etme: otonomi 0,0 koordinatına göre karar verirdi.
    if (!fighter_found || !bogey_found)
    {
        throw std::runtime_error(std::string("aircraft icinde ") + FIGHTER_ID + " veya " + BOGEY_ID + " yok");
    }

    read_missiles(data.at("missiles"), FIGHTER_ID, world.fighter.missiles);
    read_missiles(data.at("missiles"), BOGEY_ID, world.bogey.missiles);

}

void StateReader::read_flight_command(const Json& data, const std::string& id, FlightCommand& command)
{
    for (const Json& j : data.at("flight_commands"))
    {
        if (j.at("id").get<std::string>() == id)
        {
            command.heading_deg_180 = wrap_180(j.at("heading_deg").get<double>());   // 0..360 -> ±180
            command.altitude_m = j.at("altitude_m").get<double>();                   // m
            command.throttle = j.at("throttle").get<double>();                       // 0..1
            return;
        }
    }

    // Bulunamazsa sessizce 0 bırakma: uçak 0 yöne, 0 irtifaya giderdi.
    throw std::runtime_error("flight_commands icinde ucak yok: " + id);
}

void StateReader::read_aircraft(const Json& j, Aircraft& aircraft)
{
    aircraft.id = j.at("id").get<std::string>();
    aircraft.name = j.at("name").get<std::string>();
    aircraft.team = j.at("team").get<std::string>();
    aircraft.type = j.at("type").get<std::string>();
    aircraft.profile = aircraft_profile(aircraft.type);   // JSON'dan değil: type ile tablodan
    aircraft.health = j.at("health").get<double>();       // 0..1

    read_kinematics(j, aircraft.kinematics);      // uçakta konum alanları en üst seviyede
}

void StateReader::read_kinematics(const Json& j, Kinematics& k)
{
    k.lat_deg = j.at("lat_deg").get<double>();                     // derece
    k.lon_deg = j.at("lon_deg").get<double>();                     // derece
    k.altitude_m = j.at("alt_msl_m").get<double>();                // m, deniz seviyesinden
    k.heading_deg_180 = wrap_180(j.at("yaw_deg").get<double>());   // 0..360 -> ±180
    k.pitch_deg = j.at("pitch_deg").get<double>();                 // derece
    k.roll_deg = j.at("roll_deg").get<double>();                   // derece
    k.tas_mps = j.at("tas_mps").get<double>();                     // m/s
    k.mach = j.at("mach").get<double>();                           // birimsiz

    // m/s [kuzey, doğu, aşağı]: JSON'daki 3 elemanlı liste
    const Json& v = j.at("velocity_ned_mps");
    k.velocity_ned_mps[0] = v.at(0).get<double>();
    k.velocity_ned_mps[1] = v.at(1).get<double>();
    k.velocity_ned_mps[2] = v.at(2).get<double>();
}

void StateReader::read_missiles(const Json& missiles, const std::string& owner_id, std::vector<Missile>& out)
{
    // Liste her step baştan yazılır. Füze sayısı uçuş boyunca sabit olduğu için
    // pratikte aynı füzeler aynı sırayla yeniden dolar.
    out.clear();

    for (const Json& j : missiles)
    {
        if (j.at("owner_id").get<std::string>() == owner_id)
        {
            // Füze doğrudan listenin içinde doldurulur. Klasik hâli aynı işi yapar ama
            // m'yi listeye bir kez daha kopyalar (içindeki metinler ve konumla birlikte):
            //   Missile m;  read_missile(j, m);  out.push_back(m);
            out.emplace_back();             // listenin sonuna boş bir füze ekle
            read_missile(j, out.back());    // listedeki o son füzeyi doldur
        }
    }
}

void StateReader::read_missile(const Json& j, Missile& missile)
{
    missile.id = j.at("id").get<std::string>();
    missile.state = read_missile_state(j.at("state").get<std::string>());   // "ready" / "in_flight" / "ended" -> 0 / 1 / 2

    // Atılmamış füzede bu alanlar null gelir. null, string'e veya sayıya çevrilemez; önce bak.
    missile.target_id = "";
    if (!j.at("target_id").is_null())
    {
        missile.target_id = j.at("target_id").get<std::string>();
    }

    missile.outcome = "";
    if (!j.at("outcome").is_null())
    {
        missile.outcome = j.at("outcome").get<std::string>();
    }

    // Python bu ikisini yalnız füze Ready iken null yollar.
    missile.flight_time_s = 0;
    missile.kinematics = Kinematics();   // 0'larla başla
    if (missile.state != MissileState::Ready)
    {
        missile.flight_time_s = j.at("flight_time_s").get<double>();
        read_kinematics(j.at("kinematics"), missile.kinematics);
    }
}

MissileState StateReader::read_missile_state(const std::string& text)
{
    if (text == "ready")
    {
        return MissileState::Ready;
    }
    if (text == "in_flight")
    {
        return MissileState::InFlight;
    }
    if (text == "ended")
    {
        return MissileState::Ended;
    }

    throw std::runtime_error("Bilinmeyen fuze durumu: " + text);
}
