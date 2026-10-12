#include "objects.hpp"
#include "angles.hpp"

// ---------------------------------------------------------------- ölçüm

Kinematics Kinematics::from_json(const Json& j) {
    Kinematics k;
    k.lat_deg = j.at("lat_deg");
    k.lon_deg = j.at("lon_deg");
    k.alt_msl_m = j.at("alt_msl_m");
    k.yaw_deg = to_signed_heading(j.at("yaw_deg"));   // 0..360 -> ±180
    k.pitch_deg = j.at("pitch_deg");
    k.roll_deg = j.at("roll_deg");
    k.tas_mps = j.at("tas_mps");
    k.mach = j.at("mach");
    k.velocity_ned_mps = j.at("velocity_ned_mps");
    return k;
}

Aircraft Aircraft::from_json(const Json& j) {
    Aircraft a;
    a.id = j.at("id");
    a.name = j.at("name");
    a.type = j.at("type");
    a.health = j.at("health");
    a.kinematics = Kinematics::from_json(j);   // uçakta konum alanları en üst seviyede
    a.profile = aircraft_profile(a.type);      // tipe göre özellikleri tablodan al
    return a;
}

Missile Missile::from_json(const Json& j) {
    Missile m;
    m.id = j.at("id");
    m.owner_id = j.at("owner_id");
    m.state = j.at("state");

    // Atılmamış füzede bu alanlar JSON'da null gelir; null string/sayıya çevrilemez.
    if (!j.at("target_id").is_null())     m.target_id = j.at("target_id");
    if (!j.at("outcome").is_null())       m.outcome = j.at("outcome");
    if (!j.at("flight_time_s").is_null()) m.flight_time_s = j.at("flight_time_s").get<double>();
    if (!j.at("kinematics").is_null())    m.kinematics = Kinematics::from_json(j.at("kinematics"));
    return m;
}

World World::from_json(const Json& data) {
    World w;
    w.sim_time_s = data.at("sim_time_s");

    for (const Json& j : data.at("aircraft")) {
        Aircraft a = Aircraft::from_json(j);
        if (a.id == "blue") w.fighter = a;
        if (a.id == "red")  w.bogey = a;
    }

    for (const Json& j : data.at("missiles")) {
        Missile m = Missile::from_json(j);
        if (m.owner_id == "blue") w.fighter_missiles.push_back(m);
        if (m.owner_id == "red")  w.bogey_missiles.push_back(m);
    }
    return w;
}

// ---------------------------------------------------------------- komut

FlightCommand FlightCommand::from_json(const Json& j) {
    FlightCommand c;
    c.id = j.at("id");
    c.heading_deg = to_signed_heading(j.at("heading_deg"));  // 0..360 -> ±180
    c.altitude_m = j.at("altitude_m");
    c.throttle = j.at("throttle");
    return c;
}

Json FlightCommand::to_json() const {
    // NaN/sonsuz JSON'a null yazılır ve Python komutu reddeder; hatayı burada söyle.
    if (!std::isfinite(heading_deg) || !std::isfinite(altitude_m) || !std::isfinite(throttle))
        throw std::runtime_error(id + ": komut sayi degil (NaN/sonsuz)");

    return {{"id", id},
            {"heading_deg", to_compass_heading(heading_deg)},   // ±180 -> 0..360
            {"altitude_m", altitude_m},
            {"throttle", throttle}};
}
