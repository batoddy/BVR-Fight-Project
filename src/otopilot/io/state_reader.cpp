#include "io/state_reader.hpp"

#include <stdexcept>

#include "common/angles.hpp"
#include "common/roles.hpp"
#include "common/units.hpp"

void StateReader::read_world(const Json& data, World& world)
{
    world.sim_time_s = data.at("sim_time_s").get<double>();   // s

    bool fighter_found = false;
    bool hostile_found = false;

    for (const Json& json : data.at("aircraft"))
    {
        const std::string id = json.at("id").get<std::string>();
        if (id == FIGHTER_ID)
        {
            read_aircraft(json, world.fighter);
            fighter_found = true;
        }
        else if (id == HOSTILE_ID)
        {
            read_aircraft(json, world.hostile);
            hostile_found = true;
        }
    }

    if (!fighter_found || !hostile_found)   // sessizce 0'larla devam etme: otonomi 0,0 koordinatına göre karar verirdi
    {
        throw std::runtime_error(std::string("aircraft icinde ") + FIGHTER_ID + " veya " + HOSTILE_ID + " yok");
    }

    world.fighter.command = read_flight_command(data, FIGHTER_ID);
    world.hostile.command = read_flight_command(data, HOSTILE_ID);

    read_missiles(data, world.fighter);
    read_missiles(data, world.hostile);
}

FlightCommand StateReader::read_flight_command(const Json& data, const std::string& id)
{
    for (const Json& json : data.at("flight_commands"))
    {
        if (json.at("id").get<std::string>() == id)
        {
            FlightCommand command;   // g_limit Python'dan gelmez: her step yok başlar, manevra koyar
            command.set_heading_deg(json.at("heading_deg").get<double>())                // 0..360 -> ±180
                   .set_altitude_ft(meter_to_feet(json.at("altitude_m").get<double>()))  // m → ft
                   .set_throttle(json.at("throttle").get<double>());                    // 0..1
            return command;
        }
    }

    throw std::runtime_error("flight_commands icinde ucak yok: " + id);   // 0 bırakılsa uçak 0 yöne, 0 irtifaya giderdi
}

void StateReader::read_aircraft(const Json& json, Aircraft& aircraft)
{
    aircraft.id = json.at("id").get<std::string>();
    aircraft.name = json.at("name").get<std::string>();
    aircraft.team = json.at("team").get<std::string>();
    aircraft.health = json.at("health").get<double>();   // 0..1

    const std::string type = json.at("type").get<std::string>();
    if (type != aircraft.type)   // profil JSON'dan değil, tablodan; yalnız tip değişince (ilk step) kopyalanır
    {
        aircraft.type = type;
        aircraft.profile = aircraft_profile(type);
    }

    read_kinematics(json, aircraft.kinematics);   // uçakta konum alanları en üst seviyede
}

void StateReader::read_kinematics(const Json& json, Kinematics& kinematics)
{
    kinematics.lat_deg = json.at("lat_deg").get<double>();                      // derece
    kinematics.lon_deg = json.at("lon_deg").get<double>();                      // derece
    kinematics.altitude_ft = meter_to_feet(json.at("alt_msl_m").get<double>()); // m → ft
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

void StateReader::read_missiles(const Json& data, Aircraft& aircraft)
{
    if (aircraft.weapons.missiles.empty())   // ilk step: slotları kur; sayı Python'dan (ammo), tipler profilin yükünden
    {
        int count = 0;
        for (const Json& json : data.at("missiles"))
        {
            if (json.at("owner_id").get<std::string>() == aircraft.id)
            {
                count += 1;
            }
        }

        const std::vector<MissileProfile>& loadout = aircraft.profile.loadout;
        if (count > static_cast<int>(loadout.size()))
        {
            throw std::runtime_error(aircraft.id + ": Python'da " + std::to_string(count) + " fuze var, "
                                     + aircraft.type + " yukunde " + std::to_string(loadout.size())
                                     + " (aircraft_profiles.hpp loadout'u uzat ya da ammo'yu azalt)");
        }

        for (int i = 0; i < count; i++)
        {
            Missile missile;
            missile.slot = i;
            missile.profile = loadout[i];
            aircraft.weapons.missiles.push_back(missile);
        }
    }

    for (const Json& json : data.at("missiles"))   // her step: uçan ya da bitmiş füzeleri slotlara eşleştir
    {
        if (json.at("owner_id").get<std::string>() != aircraft.id)   // başka uçağın füzesi
        {
            continue;
        }
        const MissileState state = read_missile_state(json.at("state").get<std::string>());
        if (state == MissileState::Ready)                             // atılmamış füzenin bilgisi zaten slotta
        {
            continue;
        }

        const std::string python_id = json.at("id").get<std::string>();
        Missile* match = nullptr;                                     // yalnız bu fonksiyonda; saklanmaz
        for (Missile& missile : aircraft.weapons.missiles)
        {
            if (missile.python_id == python_id)                       // daha önce eşleşmiş
            {
                match = &missile;
            }
        }

        if (match == nullptr)                                         // yeni uçmaya başladı: eşleşmemiş en eski atışa bağla
        {
            for (Missile& missile : aircraft.weapons.missiles)
            {
                const bool waiting = missile.is_fired && missile.python_id.empty();
                if (waiting && (match == nullptr || missile.fire_order < match->fire_order))
                {
                    match = &missile;
                }
            }
            if (match != nullptr)
            {
                match->python_id = python_id;
            }
        }

        if (match == nullptr)                                         // weapons.fire ile atmadığımız füze: yok say
        {
            continue;
        }

        match->has_kinematics = true;
        match->state = state;
        match->target_id = json.at("target_id").get<std::string>();
        read_kinematics(json.at("kinematics"), match->kinematics);
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
