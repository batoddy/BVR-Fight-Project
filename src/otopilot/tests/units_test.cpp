// Birim geçişinin fiziksel sonuçları ve Python IO sınırını doğrular.
#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>

#include "calculations/engagement.hpp"
#include "calculations/geometry.hpp"
#include "calculations/missile_geometry.hpp"
#include "io/command_writer.hpp"
#include "io/state_reader.hpp"

static void near(double actual, double expected, const char* label, double tolerance = 1e-9)
{
    if (std::abs(actual - expected) > tolerance * std::max(1.0, std::abs(expected)))
        throw std::runtime_error(std::string(label) + ": " + std::to_string(actual)
                                 + " != " + std::to_string(expected));
}

static void require(bool condition, const char* label)
{
    if (!condition)
        throw std::runtime_error(label);
}

static void conversions_and_coordinates()
{
    near(meter_to_feet(3048), 10000, "m -> ft");
    near(feet_to_meter(10000), 3048, "ft -> m");
    near(meter_to_nmile(1852), 1, "m -> NM");
    near(nmile_to_meter(1), 1852, "NM -> m");
    near(knot_to_mps(3600), 1852, "kt -> m/s");
    near(mps_to_knot(-1852), -3600, "signed m/s -> kt");
    near(feet_to_nmile(nmile_to_feet(1)), 1, "ft / NM round trip");
    near(fps_to_knot(knot_to_fps(450)), 450, "ft/s / kt round trip");

    const Ecef equator = lla_to_ecef(0, 0, 0);
    near(equator.x_nm, 6378137.0 / 1852.0, "ECEF uses NM");
    for (double latitude : {-60.0, 0.0, 45.0, 80.0})
    {
        const Lla recovered = ecef_to_lla(lla_to_ecef(latitude, 18.0, 10000.0));
        near(recovered.lat_deg, latitude, "LLA latitude round trip");
        near(recovered.lon_deg, 18, "LLA longitude round trip");
        near(recovered.altitude_ft, 10000, "LLA altitude in ft", 1e-7);
    }
}

static void geometry_and_missile_time()
{
    Kinematics own, target;
    target.lat_deg = 1852.0 / 6371000.0 * RAD_TO_DEG;
    own.velocity_ned_kt.north = 400;
    target.velocity_ned_kt.north = -200;
    near(Geometry::range_2d_nm(own, target), 1, "one NM north");
    near(Geometry::rate_of_closure_kt(own, target), 600, "head-on closure in kt");
    near(Geometry::enemy_closing_kt(own, target), 200, "enemy closure in kt");
    target.altitude_ft = 10000;
    near(Geometry::range_3d_nm(own, target),
         std::hypot(1.0, 10000 * 0.3048 / 1852.0), "mixed altitude and horizontal units");

    target = own;
    target.altitude_ft = meter_to_feet(0.5);
    near(Geometry::los_unit(own, target).down, 0, "sub-meter LOS threshold");
    target.altitude_ft = meter_to_feet(2.0);
    near(Geometry::los_unit(own, target).down, -1, "vertical LOS normalization");
    near(MissileGeometry::time_to_go_s(60, 600), 360, "NM / kt -> seconds");
    near(MissileGeometry::time_to_go_s(60, 0), -1, "zero closure");
    near(MissileGeometry::time_to_go_s(60, -50), -1, "receding target");

    Missile missile;
    missile.profile = FOX3;
    require(MissileGeometry::pitbull(missile, meter_to_nmile(14999)), "pitbull inside");
    require(!MissileGeometry::pitbull(missile, meter_to_nmile(15001)), "pitbull outside");
}

static void engagement_units()
{
    Aircraft own, enemy;
    own.profile = aircraft_profile("F16");
    own.kinematics.altitude_ft = 10000;
    own.kinematics.tas_kt = mps_to_knot(300);
    Missile missile;
    missile.profile = FOX3;
    own.missiles.push_back(missile);
    enemy.missiles.push_back(missile);

    near(own.profile.radar_range_nm, 80000.0 / 1852.0, "radar range profile");
    near(FOX3.max_range_nm, 50000.0 / 1852.0, "missile range profile");
    near(FOX3.speed_kt, 1000.0 * 3600.0 / 1852.0, "missile speed profile");
    own.enemy.enemy_closing_kt = mps_to_knot(300);
    const Wez wez = Engagement::wez(own);
    near(wez.max_nm, 65000.0 / 1852.0, "WEZ distance over 50 seconds");
    require(Engagement::in_wez(meter_to_nmile(60000), wez), "WEZ inside");
    require(!Engagement::in_wez(meter_to_nmile(66000), wez), "WEZ outside");

    own.enemy.range_3d_nm = meter_to_nmile(40000);
    own.enemy.enemy_closing_kt = mps_to_knot(200);
    own.enemy.rate_of_closure_kt = mps_to_knot(500);
    near(Engagement::f_pole_nm(own, enemy),
         (40000.0 - 500.0 * (40000.0 / 1200.0)) / 1852.0, "F-pole seconds conversion");

}

static void metric_wire_and_imperial_calculations()
{
    Json data;
    data["sim_time_s"] = 1.0;
    data["aircraft"] = Json::array();
    data["flight_commands"] = Json::array();
    data["missiles"] = Json::array();
    for (const std::string id : {"blue", "red"})
    {
        data["aircraft"].push_back({{"id", id}, {"name", id}, {"team", id},
            {"type", "F16"}, {"health", 1}, {"lat_deg", 59}, {"lon_deg", 18},
            {"alt_msl_m", 3048}, {"yaw_deg", 270}, {"pitch_deg", 1}, {"roll_deg", 2},
            {"tas_mps", 300}, {"mach", 0.9}, {"velocity_ned_mps", {200, -30, 4}}});
        data["flight_commands"].push_back({{"id", id}, {"heading_deg", 270},
            {"altitude_m", 3048}, {"throttle", 0.7}});
        data["missiles"].push_back({{"id", id + "/missile/0"}, {"owner_id", id},
            {"state", "ready"}});
    }
    World world;
    StateReader::read_world(data, world);
    near(world.fighter.kinematics.altitude_ft, 10000, "reader altitude");
    near(world.fighter.kinematics.tas_kt, mps_to_knot(300), "reader TAS");
    near(world.fighter.kinematics.velocity_ned_kt.down, mps_to_knot(4), "reader vertical speed");
    near(world.fighter.kinematics.velocity_ned_kt.east, mps_to_knot(-30), "reader signed speed");

    FlightCommand command;
    StateReader::read_flight_command(data, "blue", command);
    near(command.altitude_ft, 10000, "reader current command");
    world.fighter.enemy.range_3d_nm = 12;
    world.fighter.enemy.rate_of_closure_kt = 500;
    const Message message = CommandWriter::write(1, world, command, command);
    near(message.data.at("aircraft").at(0).at("altitude_m").get<double>(), 3048, "writer metric command");
    const Json& calc = message.data.at("calculations").at("aircraft").at(0);
    near(calc.at("range_3d_nm").get<double>(), 12, "calculation NM field");
    near(calc.at("rate_of_closure_kt").get<double>(), 500, "calculation kt field");
    require(!calc.contains("range_3d_m"), "no metric calculation label for NM value");

    require(world.fighter.fire_missile(FOX3), "launch test missile");
    data["missiles"][0] = {{"id", "blue/missile/0"}, {"owner_id", "blue"},
        {"target_id", "red"}, {"state", "in_flight"}, {"outcome", nullptr},
        {"kinematics", data["aircraft"][0]}};
    StateReader::read_world(data, world);
    const Missile& missile = world.fighter.missiles[0];
    require(missile.has_kinematics, "missile kinematics matched");
    near(missile.kinematics.altitude_ft, 10000, "missile reader altitude");
    near(missile.kinematics.tas_kt, mps_to_knot(300), "missile reader TAS");
}

int main()
{
    try
    {
        conversions_and_coordinates();
        geometry_and_missile_time();
        engagement_units();
        metric_wire_and_imperial_calculations();
        std::cout << "Unit conversion, geometry, engagement and IO checks passed.\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
