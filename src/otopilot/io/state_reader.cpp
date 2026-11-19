#include "io/state_reader.hpp"

#include <stdexcept>

#include "common/angles.hpp"
#include "io/roles.hpp"
#include "common/units.hpp"

namespace
{
    const Json& find_hello_aircraft(const Json& data, const std::string& id)   // hello aircraft listesinde id; yoksa hata
    {
        for (const Json& json : data.at("aircraft"))
        {
            if (json.at("id").get<std::string>() == id)
            {
                return json;
            }
        }
        throw std::runtime_error("hello aircraft icinde ucak yok: " + id);
    }

    const Json& find_packet(const Json& data, const std::string& id)   // state aircraft nesnesinde id'nin paketi; yoksa hata
    {
        const Json& aircraft = data.at("aircraft");
        if (!aircraft.contains(id))   // sessizce 0'larla devam etme: otonomi 0,0 koordinatına göre karar verirdi
        {
            throw std::runtime_error("state aircraft icinde paket yok: " + id);
        }
        return aircraft.at(id);
    }
}

void StateReader::read_hello(const Json& data, World& world)
{
    read_hello_aircraft(find_hello_aircraft(data, FIGHTER_ID), world.fighter);
    read_hello_aircraft(find_hello_aircraft(data, HOSTILE_ID), world.hostile);
}

void StateReader::read_hello_aircraft(const Json& json, Aircraft& aircraft)
{
    aircraft.id = json.at("id").get<std::string>();
    aircraft.name = json.at("name").get<std::string>();
    aircraft.team = json.at("team").get<std::string>();
    aircraft.type = json.at("type").get<std::string>();
    aircraft.profile = aircraft_profile(aircraft.type);   // tabloda yoksa hata

    aircraft.weapons.missiles.clear();
    for (const Json& weapon : json.at("weapons"))         // slot sırası scenario.yaml ammo sırası
    {
        Missile missile;
        missile.slot = weapon.at("slot").get<int>();
        if (missile.slot != static_cast<int>(aircraft.weapons.missiles.size()))
        {
            throw std::runtime_error(aircraft.id + ": hello slotlari 0'dan sirali degil");
        }
        missile.profile = missile_profile(weapon.at("designation").get<std::string>());   // tabloda yoksa hata
        aircraft.weapons.missiles.push_back(missile);
    }
}

// Her uçak kendi paketinden: own ve weapons. warnings, events, contacts ve rwr okunmaz: düşman gerçek kinematiğiyle (World::update).
void StateReader::read_world(const Json& data, World& world)
{
    if (world.fighter.id.empty() || world.hostile.id.empty())
    {
        throw std::runtime_error("state'ten once hello okunmali");
    }

    world.sim_time_s = data.at("time_s").get<double>();   // s

    read_packet(find_packet(data, FIGHTER_ID), world.fighter);
    read_packet(find_packet(data, HOSTILE_ID), world.hostile);
}

void StateReader::read_packet(const Json& packet, Aircraft& aircraft)
{
    const Json& own = packet.at("own");
    if (own.at("id").get<std::string>() != aircraft.id)
    {
        throw std::runtime_error("Paket baska ucaga ait: " + own.at("id").get<std::string>());
    }

    aircraft.health = own.at("health").get<double>();   // 0..1
    read_kinematics(own.at("kinematics"), aircraft.kinematics);
    aircraft.command = read_flight_command(own.at("applied_command"));

    read_weapons(packet.at("weapons"), aircraft);
}

FlightCommand StateReader::read_flight_command(const Json& json)
{
    FlightCommand command;   // g_limit okunmaz: her step yok başlar, manevra koyar
    command.set_heading_deg(json.at("heading_deg").get<double>())                // 0..360 -> ±180; direct modda da son hedef gelir
           .set_altitude_ft(meter_to_feet(json.at("altitude_m").get<double>()))  // m → ft
           .set_throttle(json.at("throttle").get<double>());                    // 0..1

    if (json.value("mode", "target") == "direct")   // alan yoksa target (v2)
    {
        command.set_bank_deg(json.at("bank_deg").get<double>())   // derece
               .set_g(json.at("g").get<double>());                // g
    }
    return command;
}

void StateReader::read_kinematics(const Json& json, Kinematics& kinematics)
{
    kinematics.lat_deg = json.at("lat_deg").get<double>();                      // derece
    kinematics.lon_deg = json.at("lon_deg").get<double>();                      // derece
    kinematics.altitude_ft = meter_to_feet(json.at("alt_m").get<double>());     // m → ft
    kinematics.heading_deg = wrap_180(json.at("yaw_deg").get<double>());        // 0..360 -> ±180
    kinematics.pitch_deg = json.at("pitch_deg").get<double>();                  // derece
    kinematics.roll_deg = json.at("roll_deg").get<double>();                    // derece
    kinematics.tas_kt = mps_to_knot(json.at("tas_mps").get<double>());          // m/s → kt
    kinematics.mach = json.at("mach").get<double>();                            // birimsiz

    const Json& velocity = json.at("velocity_ned_mps");                         // m/s → kt, üç bileşen
    kinematics.velocity_ned_kt.north = mps_to_knot(velocity.at(0).get<double>());
    kinematics.velocity_ned_kt.east = mps_to_knot(velocity.at(1).get<double>());
    kinematics.velocity_ned_kt.down = mps_to_knot(velocity.at(2).get<double>());
}

void StateReader::read_weapons(const Json& weapons, Aircraft& aircraft)
{
    std::vector<Missile>& missiles = aircraft.weapons.missiles;
    if (weapons.size() != missiles.size())
    {
        throw std::runtime_error(aircraft.id + ": state'te " + std::to_string(weapons.size()) + " slot var, hello'da "
                                 + std::to_string(missiles.size()));
    }

    for (const Json& json : weapons)
    {
        const int slot = json.at("slot").get<int>();
        if (slot < 0 || slot >= static_cast<int>(missiles.size()))
        {
            throw std::runtime_error(aircraft.id + ": gecersiz slot " + std::to_string(slot));
        }
        Missile& missile = missiles[slot];

        const bool was_ready = missile.state == MissileState::Ready;
        missile.state = read_missile_state(json.at("state").get<std::string>());
        if (was_ready && missile.state != MissileState::Ready)   // C++'ın emri olmadan atılan da sayılır
        {
            aircraft.weapons.record_launch(missile);
        }
        missile.guidance_mode = read_guidance_mode(json.at("guidance_mode").get<std::string>());
        missile.outcome = read_outcome(json.at("outcome"));
        missile.target.is_pitbull = json.at("is_pitbull").get<bool>();   // sim'de bir kez true olunca kalır
        missile.target.is_guided = json.at("is_locked").get<bool>();     // datalink ya da seeker

        const Json& target_id = json.at("target_id");
        missile.target_id = target_id.is_string() ? target_id.get<std::string>() : "";

        const Json& kinematics = json.at("kinematics");
        missile.has_kinematics = !kinematics.is_null();                  // hazır slotta null
        if (missile.has_kinematics)
        {
            read_kinematics(kinematics, missile.kinematics);
        }
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

GuidanceMode StateReader::read_guidance_mode(const std::string& text)
{
    if (text == "none")
    {
        return GuidanceMode::None;
    }
    if (text == "datalink")
    {
        return GuidanceMode::Datalink;
    }
    if (text == "inertial")
    {
        return GuidanceMode::Inertial;
    }
    if (text == "seeker")
    {
        return GuidanceMode::Seeker;
    }

    throw std::runtime_error("Bilinmeyen gudum kipi: " + text);
}

std::optional<MissileOutcome> StateReader::read_outcome(const Json& json)
{
    if (json.is_null())
    {
        return std::nullopt;
    }
    const std::string text = json.get<std::string>();
    if (text == "hit")
    {
        return MissileOutcome::Hit;
    }
    if (text == "lost")
    {
        return MissileOutcome::Lost;
    }

    throw std::runtime_error("Bilinmeyen fuze sonucu: " + text);
}
