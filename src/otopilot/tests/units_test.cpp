// Birim dönüşümleri, saf hesaplar, World::update ve Python IO sınırı testleri.
#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>

#include "calculations/engagement.hpp"
#include "calculations/geometry.hpp"
#include "calculations/missile_geometry.hpp"
#include "calculations/turn.hpp"
#include "common/angles.hpp"
#include "common/geodesy.hpp"
#include "common/units.hpp"
#include "io/command_writer.hpp"
#include "io/state_reader.hpp"
#include "model/flight_command.hpp"
#include "model/world.hpp"

namespace
{
    void near(double actual, double expected, const char* label, double tolerance = 1e-9)
    {
        if (std::abs(actual - expected) > tolerance * std::max(1.0, std::abs(expected)))
        {
            throw std::runtime_error(std::string(label) + ": " + std::to_string(actual)
                                     + " != " + std::to_string(expected));
        }
    }

    void require(bool condition, const char* label)
    {
        if (!condition)
        {
            throw std::runtime_error(label);
        }
    }

    // own'dan bearing_deg yönünde range_nm uzağa bir nokta (geometry'nin düz dünya yaklaşımıyla).
    Kinematics place(const Kinematics& own, double bearing_deg, double range_nm)
    {
        const double earth_radius_nm = meter_to_nmile(6371000.0);
        Kinematics other = own;
        other.lat_deg = own.lat_deg + range_nm * std::cos(bearing_deg * DEG_TO_RAD) / earth_radius_nm * RAD_TO_DEG;
        const double mean_lat_rad = (own.lat_deg + other.lat_deg) / 2.0 * DEG_TO_RAD;
        other.lon_deg = own.lon_deg + range_nm * std::sin(bearing_deg * DEG_TO_RAD)
                        / (earth_radius_nm * std::cos(mean_lat_rad)) * RAD_TO_DEG;
        return other;
    }

    void conversions_and_coordinates()
    {
        near(meter_to_feet(3048), 10000, "m -> ft");
        near(feet_to_meter(10000), 3048, "ft -> m");
        near(meter_to_nmile(1852), 1, "m -> NM");
        near(nmile_to_meter(1), 1852, "NM -> m");
        near(knot_to_mps(3600), 1852, "kt -> m/s");
        near(mps_to_knot(-1852), -3600, "signed m/s -> kt");
        near(feet_to_nmile(nmile_to_feet(1)), 1, "ft / NM round trip");
        near(fps_to_knot(knot_to_fps(450)), 450, "ft/s / kt round trip");
        near(wrap_180(270), -90, "wrap 270");
        near(wrap_180(-200), 160, "wrap -200");
        near(wrap_360(-90), 270, "wrap -90");

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

    void geometry_and_missile_time()
    {
        Kinematics own, other;
        other.lat_deg = 1852.0 / 6371000.0 * RAD_TO_DEG;
        own.velocity_ned_kt.north = 400;
        other.velocity_ned_kt.north = -200;
        near(geometry::range_2d_nm(own, other), 1, "one NM north");
        near(geometry::closure_kt(own, other), 600, "head-on closure in kt");
        near(geometry::other_closure_kt(own, other), 200, "other closure in kt");
        other.altitude_ft = 10000;
        const double altitude_difference_nm = 10000 * 0.3048 / 1852.0;
        near(geometry::range_3d_nm(own, other),
             std::sqrt(1.0 * 1.0 + altitude_difference_nm * altitude_difference_nm), "mixed altitude and horizontal units");

        other = own;
        other.altitude_ft = meter_to_feet(0.5);
        near(geometry::los_unit(own, other).down, 0, "sub-meter LOS threshold");
        other.altitude_ft = meter_to_feet(2.0);
        near(geometry::los_unit(own, other).down, -1, "vertical LOS normalization");

        near(*missile_geometry::time_to_go_s(60, 600), 360, "NM / kt -> seconds");
        require(!missile_geometry::time_to_go_s(60, 0).has_value(), "zero closure: no time to go");
        require(!missile_geometry::time_to_go_s(60, -50).has_value(), "receding target: no time to go");

        Missile missile;
        missile.profile = AIM120C;
        require(missile_geometry::is_pitbull(missile, AIM120C.pitbull_range_nm - 0.01), "pitbull inside");
        require(!missile_geometry::is_pitbull(missile, AIM120C.pitbull_range_nm + 0.01), "pitbull outside");
    }

    void flight_command_setters()
    {
        FlightCommand command;
        command.set_heading_deg(270).set_throttle(1.4).set_g_limit(12);
        near(command.heading_deg(), -90, "heading wrapped to ±180");
        near(command.throttle(), 1.0, "throttle clamped");
        near(*command.g_limit(), FlightCommand::G_LIMIT_MAX, "g limit clamped");
        command.clear_g_limit();
        require(!command.g_limit().has_value(), "g limit cleared");
    }

    void turn_and_out_plan()
    {
        // 90° sağa dönüş: kuzeye giderken doğuya dönülür; yer değişimi kuzeye r, doğuya r.
        Kinematics own;
        own.tas_kt = 500;
        const TurnResult right = turn::to_heading(own, 90, 6.0, 500);
        const double radius_nm = meter_to_nmile(turn::radius_m(500, 6.0));
        near(right.turn_deg, 90, "turn right 90");
        near(right.displacement_nm.north, radius_nm, "right turn north displacement");
        near(right.displacement_nm.east, radius_nm, "right turn east displacement");
        near(right.time_s, (PI / 2) * turn::radius_m(500, 6.0) / knot_to_mps(500), "turn time");

        const TurnResult left = turn::to_heading(own, -90, 6.0, 500);
        near(left.displacement_nm.east, -radius_nm, "left turn east displacement");

        // Out planı eski out formülüyle aynı sonucu vermeli:
        //   x = -r·sin|ATA|, y = r·(1 + cos|ATA|), yaklaşma = R - √((R - x)² + y²), Δt = (180 - |ATA|) · r / V
        Aircraft fighter, hostile;
        fighter.profile = aircraft_profile("F16");
        fighter.kinematics.lat_deg = 59.5;
        fighter.kinematics.lon_deg = 18.0;
        fighter.kinematics.heading_deg = 20;
        fighter.kinematics.tas_kt = 600;
        hostile.kinematics = place(fighter.kinematics, 55, 15);   // ATA +35, düşman sağda

        const OutPlan out = engagement::out_plan(fighter, hostile);
        const double range_nm = geometry::range_2d_nm(fighter.kinematics, hostile.kinematics);
        const double ata_rad = std::fabs(geometry::ata_deg(fighter.kinematics, hostile.kinematics)) * DEG_TO_RAD;
        const double r_m = turn::radius_m(600, fighter.profile.out_g);
        const double x_m = -r_m * std::sin(ata_rad);
        const double y_m = r_m * (1.0 + std::cos(ata_rad));
        const double range_m = nmile_to_meter(range_nm);
        const double legacy_approach_nm = meter_to_nmile(range_m - std::sqrt((range_m - x_m) * (range_m - x_m) + y_m * y_m));
        const double legacy_time_s = (PI - ata_rad) * r_m / knot_to_mps(600);
        near(out.approach_nm, legacy_approach_nm, "out approach equals legacy formula", 1e-9);
        near(out.turn.time_s, legacy_time_s, "out time equals legacy formula", 1e-9);
        require(out.turn.turn_deg < 0, "enemy on the right: out turns left");
    }

    void engagement_values()
    {
        // İki F16 (ana silah AIM-120C): fighter 10 km'de 700 kt kuzeye, hostile 20 NM kuzeyde bana doğru 300 kt.
        Aircraft fighter, hostile;
        fighter.profile = aircraft_profile("F16");
        hostile.profile = aircraft_profile("F16");
        fighter.kinematics.lat_deg = 59.5;
        fighter.kinematics.lon_deg = 18.0;
        fighter.kinematics.altitude_ft = meter_to_feet(10000);
        fighter.kinematics.tas_kt = 700;
        fighter.kinematics.velocity_ned_kt.north = 700;
        hostile.kinematics = place(fighter.kinematics, 0, 20);
        hostile.kinematics.heading_deg = 180;
        hostile.kinematics.velocity_ned_kt.north = -300;

        const double range_nm = geometry::range_3d_nm(fighter.kinematics, hostile.kinematics);
        const double enemy_closure_kt = geometry::other_closure_kt(fighter.kinematics, hostile.kinematics);
        const double closure_kt = geometry::closure_kt(fighter.kinematics, hostile.kinematics);

        // WEZ: düşman füzenin uçuş süresince yaklaşırsa menzil uzar.
        const Wez wez = engagement::wez(fighter, hostile);
        const double flight_time_h = AIM120C.max_range_nm / AIM120C.speed_kt;
        near(wez.max_nm, AIM120C.max_range_nm + enemy_closure_kt * flight_time_h, "WEZ grows with enemy closure");
        require(wez.max_nm > AIM120C.max_range_nm, "WEZ longer when the enemy approaches");
        require(engagement::in_wez(fighter, hostile) == (range_nm > wez.min_nm && range_nm < wez.max_nm), "in WEZ");

        // R_cold: 10 km ve 700 kt'de safety · e^a
        near(engagement::r_cold_nm(fighter, hostile),
             AIM120C_R_COLD.safety * std::exp(AIM120C_R_COLD.a), "R_cold at reference point");

        // F-pole: füze varana kadar iki uçağın kapattığı mesafe çıkar.
        const std::optional<double> f_pole = engagement::f_pole_nm(fighter, hostile);
        near(*f_pole, range_nm - closure_kt * (range_nm / (AIM120C.speed_kt + enemy_closure_kt)), "F-pole");

        // NEZ = R_cold + yaklaşma + dönerken kaçamadığım yol; MAR bunun üstüne patlama yarıçapı ve out payı ekler.
        const OutPlan out = engagement::out_plan(fighter, hostile);
        const double nez_nm = engagement::nez_nm(fighter, hostile);
        near(nez_nm, engagement::r_cold_nm(fighter, hostile) + out.approach_nm
                     + travel_distance_nm(out.escape_kt, out.turn.time_s), "NEZ");
        require(engagement::mar_nm(fighter, hostile)
                >= nez_nm + feet_to_nmile(AIM120C.blast_radius_ft) + fighter.profile.out_offset_nm, "MAR covers NEZ");

        // DOR = MAR + düşmanın 180°'lik recommit dönüşü (π·r / V) boyunca yaklaştığı yol.
        const double recommit_turn_s = PI * turn::radius_m(700, fighter.profile.out_g) / knot_to_mps(700);
        near(engagement::dor_nm(fighter, hostile),
             engagement::mar_nm(fighter, hostile) + enemy_closure_kt * recommit_turn_s / 3600.0, "DOR");

        // Düşman füzeden hızlı kaçıyorsa füze yetişemez: F-pole yok.
        hostile.kinematics.velocity_ned_kt.north = AIM120C.speed_kt + 100;
        require(!engagement::f_pole_nm(fighter, hostile).has_value(), "F-pole when the missile cannot catch the target");
    }

    void world_threat_selection()
    {
        World world;
        world.fighter.id = "blue";
        world.hostile.id = "red";
        world.fighter.profile = aircraft_profile("F16");
        world.hostile.profile = aircraft_profile("F16");
        world.fighter.kinematics.lat_deg = 59.5;
        world.fighter.kinematics.lon_deg = 18.0;
        world.fighter.kinematics.tas_kt = 500;
        world.hostile.kinematics = place(world.fighter.kinematics, 0, 30);
        world.hostile.kinematics.heading_deg = 180;

        // Hostile'ın iki füzesi bana geliyor: yakın olan daha acil.
        for (int slot = 0; slot < 2; slot++)
        {
            Missile missile;
            missile.slot = slot;
            missile.profile = AIM120C;
            missile.is_fired = true;
            missile.has_kinematics = true;
            missile.state = MissileState::InFlight;
            missile.target_id = "blue";
            missile.kinematics = place(world.fighter.kinematics, 0, slot == 0 ? 12 : 6);
            missile.kinematics.velocity_ned_kt.north = -1500;
            world.hostile.weapons.missiles.push_back(missile);
        }

        world.update();
        require(world.fighter.enemy_id == "red", "enemy assigned");
        require(world.fighter.picture.threat.has_value(), "threat found");
        require(world.fighter.picture.threat->slot == 1, "closest missile is the threat");
        near(world.fighter.picture.threat->bearing_deg, 0, "threat bearing", 1e-6);
        require(!world.hostile.picture.threat.has_value(), "no missile against hostile");
        // Silah ve kaçış hesapları main_weapon'dan; füze slotu olmasa da hesaplanır.
        require(world.hostile.picture.mar_nm > 0, "MAR from main weapon without missile slots");
        near(world.fighter.weapons.wez.min_nm, AIM120C.min_range_nm, "WEZ from main weapon without missile slots");

        // hostile'ın NEZ'i = fighter'ın kendi kaçış hesabındaki NEZ (aynı füze, aynı out planı); simetri.
        near(world.hostile.weapons.nez_nm, world.fighter.picture.nez_nm, "weapon NEZ mirrors enemy escape NEZ", 1e-9);
        near(world.fighter.weapons.nez_nm, world.hostile.picture.nez_nm, "weapon NEZ mirrors enemy escape NEZ (other side)", 1e-9);
        require(world.hostile.weapons.f_pole_nm.has_value(), "F-pole computed");
        require(world.fighter.picture.me_in_enemy_wez == world.hostile.weapons.enemy_in_wez, "me_in_enemy_wez copied");
        const double range_nm = world.hostile.picture.range_3d_nm;
        require(world.hostile.weapons.enemy_in_nez == (range_nm <= world.hostile.weapons.nez_nm), "enemy in NEZ");
    }

    void metric_wire_and_imperial_calculations()
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
        near(world.fighter.kinematics.heading_deg, -90, "reader heading ±180");
        near(world.fighter.kinematics.tas_kt, mps_to_knot(300), "reader TAS");
        near(world.fighter.kinematics.velocity_ned_kt.down, mps_to_knot(4), "reader vertical speed");
        near(world.fighter.kinematics.velocity_ned_kt.east, mps_to_knot(-30), "reader signed speed");
        near(world.fighter.command.altitude_ft(), 10000, "reader current command");
        near(world.fighter.command.heading_deg(), -90, "reader command heading ±180");

        world.fighter.picture.range_3d_nm = 12;
        world.fighter.picture.closure_kt = 500;
        world.fighter.command.set_g_limit(6.0);
        const Message message = CommandWriter::write(1, world);
        const Json& fighter_command = message.data.at("aircraft").at(0);
        near(fighter_command.at("altitude_m").get<double>(), 3048, "writer metric command");
        near(fighter_command.at("heading_deg").get<double>(), 270, "writer heading 0..360");
        near(fighter_command.at("g_limit").get<double>(), 6.0, "writer g limit");
        require(!message.data.at("aircraft").at(1).contains("g_limit"), "no g limit when unset");
        const Json& calc = message.data.at("calculations").at("aircraft").at(0);
        near(calc.at("range_3d_nm").get<double>(), 12, "calculation NM field");
        near(calc.at("rate_of_closure_kt").get<double>(), 500, "calculation kt field (wire name)");
        require(!calc.contains("range_3d_m"), "no metric calculation label for NM value");

        require(world.fighter.weapons.fire(AIM120C), "launch test missile");
        require(world.fighter.weapons.has_launch_order, "launch order set");
        data["missiles"][0] = {{"id", "blue/missile/0"}, {"owner_id", "blue"},
            {"target_id", "red"}, {"state", "in_flight"}, {"outcome", nullptr},
            {"kinematics", data["aircraft"][0]}};
        StateReader::read_world(data, world);
        require(!world.fighter.weapons.has_launch_order, "launch order reset each step");
        const Missile& missile = world.fighter.weapons.missiles[0];
        require(missile.has_kinematics, "missile kinematics matched");
        require(world.fighter.weapons.active_missile() == &missile, "active missile");
        near(missile.kinematics.altitude_ft, 10000, "missile reader altitude");
        near(missile.kinematics.tas_kt, mps_to_knot(300), "missile reader TAS");
    }
}

int main()
{
    try
    {
        conversions_and_coordinates();
        geometry_and_missile_time();
        flight_command_setters();
        turn_and_out_plan();
        engagement_values();
        world_threat_selection();
        metric_wire_and_imperial_calculations();
        std::cout << "Unit conversion, geometry, turn, engagement, world and IO checks passed.\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
