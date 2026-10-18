#include "state_reader.hpp"
#include <stdexcept>
#include "angles.hpp"
#include "roles.hpp"

void StateReader::read_world(const Json& data, World& world)
{
    world.sim_time_s = data.at("sim_time_s");   // s

    bool fighter_found = false;
    bool bogey_found = false;

    for (const Json& j : data.at("aircraft"))
    {
        if (j.at("id") == FIGHTER_ID)
        {
            read_aircraft(j, world.fighter);
            fighter_found = true;
        }
        else if (j.at("id") == BOGEY_ID)
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
        if (j.at("id") == id)
        {
            command.heading_deg_180 = wrap_180(j.at("heading_deg"));   // 0..360 -> ±180
            command.altitude_m = j.at("altitude_m");                   // m
            command.throttle = j.at("throttle");                       // 0..1
            return;
        }
    }

    // Bulunamazsa sessizce 0 bırakma: uçak 0 yöne, 0 irtifaya giderdi.
    throw std::runtime_error("flight_commands icinde ucak yok: " + id);
}

void StateReader::read_aircraft(const Json& j, Aircraft& aircraft)
{
    aircraft.id = j.at("id");
    aircraft.name = j.at("name");
    aircraft.team = j.at("team");
    aircraft.type = j.at("type");
    aircraft.profile = aircraft_profile(aircraft.type);   // JSON'dan değil: type ile tablodan
    aircraft.health = j.at("health");             // 0..1

    read_kinematics(j, aircraft.kinematics);      // uçakta konum alanları en üst seviyede
}

void StateReader::read_kinematics(const Json& j, Kinematics& k)
{
    k.lat_deg = j.at("lat_deg");                       // derece
    k.lon_deg = j.at("lon_deg");                       // derece
    k.alt_msl_m = j.at("alt_msl_m");                   // m
    k.heading_deg_180 = wrap_180(j.at("yaw_deg"));     // 0..360 -> ±180
    k.pitch_deg = j.at("pitch_deg");                   // derece
    k.roll_deg = j.at("roll_deg");                     // derece
    k.tas_mps = j.at("tas_mps");                       // m/s
    k.mach = j.at("mach");                             // birimsiz
    k.velocity_ned_mps = j.at("velocity_ned_mps");     // m/s [kuzey, doğu, aşağı]
}

void StateReader::read_missiles(const Json& missiles, const std::string& owner_id, std::vector<Missile>& out)
{
    // Liste her step baştan yazılır. Füze sayısı uçuş boyunca sabit olduğu için
    // pratikte aynı füzeler aynı sırayla yeniden dolar.
    out.clear();

    for (const Json& j : missiles)
    {
        if (j.at("owner_id") == owner_id)
        {
            out.emplace_back();             // listenin sonuna boş bir füze ekle
            read_missile(j, out.back());    // ve içini doldur
        }
    }
}

void StateReader::read_missile(const Json& j, Missile& missile)
{
    missile.id = j.at("id");
    missile.state = j.at("state");   // "ready" / "in_flight" / "ended"

    // Atılmamış füzede bu alanlar null gelir. null, string'e veya sayıya çevrilemez; önce bak.
    missile.target_id = "";
    if (!j.at("target_id").is_null())
    {
        missile.target_id = j.at("target_id");
    }

    missile.outcome = "";
    if (!j.at("outcome").is_null())
    {
        missile.outcome = j.at("outcome");
    }

    missile.flight_time_s.reset();   // optional'ı boşalt
    if (!j.at("flight_time_s").is_null())
    {
        missile.flight_time_s = j.at("flight_time_s").get<double>();
    }

    missile.kinematics.reset();
    if (!j.at("kinematics").is_null())
    {
        missile.kinematics.emplace();                               // içine boş bir Kinematics koy
        read_kinematics(j.at("kinematics"), *missile.kinematics);   // ve doldur
    }
}
