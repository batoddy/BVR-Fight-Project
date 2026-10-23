#include "io/state_reader.hpp"
#include <stdexcept>
#include "common/util_functions.hpp"
#include "common/roles.hpp"

void StateReader::read_world(const Json& data, World& world)
{
    world.sim_time_s = data.at("sim_time_s").get<double>();   // s

    bool fighter_found = false;
    bool hostile_found = false;

    for (const Json& j : data.at("aircraft"))
    {
        std::string id = j.at("id").get<std::string>();
        if (id == FIGHTER_ID)
        {
            read_aircraft(j, world.fighter);
            fighter_found = true;
        }
        else if (id == HOSTILE_ID)
        {
            read_aircraft(j, world.hostile);
            hostile_found = true;
        }
    }

    // Biri eksikse sessizce 0'larla devam etme: otonomi 0,0 koordinatına göre karar verirdi.
    if (!fighter_found || !hostile_found)
    {
        throw std::runtime_error(std::string("aircraft icinde ") + FIGHTER_ID + " veya " + HOSTILE_ID + " yok");
    }

    read_missiles(data, world.fighter);
    read_missiles(data, world.hostile);
}

void StateReader::read_flight_command(const Json& data, const std::string& id, FlightCommand& command)
{
    for (const Json& j : data.at("flight_commands"))
    {
        if (j.at("id").get<std::string>() == id)
        {
            command.heading_deg_180 = wrap_180(j.at("heading_deg").get<double>());   // 0..360 -> ±180
            command.altitude_ft = meter_to_feet(j.at("altitude_m").get<double>()); // m → ft
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
    k.altitude_ft = meter_to_feet(j.at("alt_msl_m").get<double>()); // m → ft
    k.heading_deg_180 = wrap_180(j.at("yaw_deg").get<double>());   // 0..360 -> ±180
    k.pitch_deg = j.at("pitch_deg").get<double>();                 // derece
    k.roll_deg = j.at("roll_deg").get<double>();                   // derece
    k.tas_kt = mps_to_knot(j.at("tas_mps").get<double>());         // m/s → KTAS
    k.mach = j.at("mach").get<double>();                           // birimsiz

    // Python m/s gönderir; üç NED hız bileşenini de knot'a çevir.
    const Json& v = j.at("velocity_ned_mps");
    k.velocity_ned_kt.north = mps_to_knot(v.at(0).get<double>());
    k.velocity_ned_kt.east = mps_to_knot(v.at(1).get<double>());
    k.velocity_ned_kt.down = mps_to_knot(v.at(2).get<double>());

}

void StateReader::read_missiles(const Json& data, Aircraft& aircraft)
{
    aircraft.launch_order = false;   // yeni step: önceki step'in emri gitti

    // İlk step: slotları kur. Sayı Python'dan (scenario.yaml ammo), tipler uçak profilinin yükünden.
    if (aircraft.missiles.empty())
    {
        int count = 0;
        for (const Json& j : data.at("missiles"))
        {
            if (j.at("owner_id").get<std::string>() == aircraft.id)
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
            Missile m;
            m.slot = i;
            m.profile = loadout[i];
            aircraft.missiles.push_back(m);
        }
    }

    // Her step: Python'da uçan ya da bitmiş füzeleri slotlarımıza eşleştir ve doldur.
    for (const Json& j : data.at("missiles"))
    {
        // Başka uçağın füzesi bizi ilgilendirmez.
        if (j.at("owner_id").get<std::string>() == aircraft.id)
        {
            MissileState state = read_missile_state(j.at("state").get<std::string>());

            // Atılmamış füzenin bilgisi zaten slotta.
            if (state != MissileState::Ready)
            {
                // Bu Python füzesi hangi slotumuz? Önce daha önce eşleşmiş mi diye bak.
                std::string python_id = j.at("id").get<std::string>();
                Missile* match = nullptr;   // yalnız bu fonksiyon içinde kullanılır; saklanmaz
                for (Missile& m : aircraft.missiles)
                {
                    if (m.python_id == python_id)
                    {
                        match = &m;
                    }
                }

                // Eşleşmemişse yeni uçmaya başlamıştır: atılmış ama henüz eşleşmemiş en eski atışa bağla.
                if (match == nullptr)
                {
                    for (Missile& m : aircraft.missiles)
                    {
                        bool waiting = m.fired && m.python_id.empty();
                        if (waiting && (match == nullptr || m.fire_order < match->fire_order))
                        {
                            match = &m;
                        }
                    }
                    if (match != nullptr)
                    {
                        match->python_id = python_id;
                    }
                }

                // Hâlâ eşleşmediyse bizim fire_missile ile atmadığımız bir füzedir: yok say.
                if (match != nullptr)
                {
                    match->has_kinematics = true;
                    match->state = state;
                    match->target_id = j.at("target_id").get<std::string>();

                    match->outcome = MissileOutcome::None;
                    if (!j.at("outcome").is_null())
                    {
                        std::string outcome = j.at("outcome").get<std::string>();
                        match->outcome = (outcome == "hit") ? MissileOutcome::Hit : MissileOutcome::Lost;
                    }
                    read_kinematics(j.at("kinematics"), match->kinematics);
                }
            }
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
