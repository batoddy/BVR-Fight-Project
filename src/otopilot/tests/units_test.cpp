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
#include "common/formulas.hpp"
#include "common/units.hpp"
#include "control/conditions.hpp"
#include "control/otopilot.hpp"
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

        Missile missile;
        missile.profile = AIM120C;
        require(missile_geometry::is_pitbull(missile, AIM120C.pitbull_range_nm - 0.01), "pitbull inside");
        require(!missile_geometry::is_pitbull(missile, AIM120C.pitbull_range_nm + 0.01), "pitbull outside");
    }

    void flight_command_setters()
    {
        FlightCommand command;
        command.set_heading_deg(270).set_throttle(1.4).set_g_limit(12, 9.0);
        near(command.heading_deg(), -90, "heading wrapped to ±180");
        near(command.throttle(), 1.0, "throttle clamped");
        near(*command.g_limit(), 9.0, "g limit clamped to profile max");
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

    void maneuver_lifecycle()
    {
        class CountingManeuver : public Maneuver
        {
        public:
            int entries = 0;
            int updates = 0;

            Phase phase() const override { return Phase::Hold; }
            void enter(const Aircraft&) override { ++entries; }
            FlightCommand update(const Aircraft& own) override
            {
                require(entries > 0, "enter precedes first update");
                ++updates;
                return own.command;
            }
        };

        Aircraft own;
        Maneuvers maneuvers, other_pilot;
        CountingManeuver first, second, other;
        maneuvers.fly(own, first);
        maneuvers.fly(own, first);
        require(first.entries == 1 && first.updates == 2, "continuous maneuver enters once");
        maneuvers.fly(own, second);
        require(second.entries == 1, "different object with same phase enters");
        other_pilot.fly(own, other);
        maneuvers.fly(own, second);
        require(second.entries == 1, "other pilot does not change active maneuver");
        maneuvers.fly(own, first);
        require(first.entries == 2, "returning to a maneuver enters again");

        own.profile = aircraft_profile("F16");
        own.kinematics.altitude_ft = 30000;
        maneuvers.fly(own, maneuvers.out);
        near(own.command.altitude_ft(), 26700, "first out locks altitude");
        own.kinematics.altitude_ft = 28000;
        maneuvers.fly(own, maneuvers.out);
        near(own.command.altitude_ft(), 26700, "continuous out keeps locked altitude");
        maneuvers.fly(own, maneuvers.commit);
        maneuvers.fly(own, maneuvers.out);
        near(own.command.altitude_ft(), 24700, "second out uses new entry altitude");

        own.picture.ata_deg = 20;
        own.picture.ta_deg = 20;
        maneuvers.fly(own, maneuvers.crank);
        require(maneuvers.crank.side() == ManeuverSide::Left, "first crank chooses side");
        own.picture.ata_deg = -20;
        own.picture.ta_deg = -20;
        maneuvers.fly(own, maneuvers.crank);
        require(maneuvers.crank.side() == ManeuverSide::Left, "continuous crank keeps side");
        maneuvers.fly(own, maneuvers.commit);
        maneuvers.fly(own, maneuvers.crank);
        require(maneuvers.crank.side() == ManeuverSide::Right, "second crank chooses side again");
    }

    void otopilot_flow()
    {
        Aircraft own, enemy;
        own.profile = aircraft_profile("F16");
        own.kinematics.altitude_ft = 30000;
        own.command.set_altitude_ft(30000).set_throttle(0.45);
        own.picture.range_3d_nm = 34;
        for (int slot = 0; slot < 4; ++slot)
        {
            Missile missile;
            missile.slot = slot;
            missile.profile = AIM120C;
            own.weapons.missiles.push_back(missile);
        }

        Otopilot pilot{Mission{MissionGoal::Destroy, RiskLevel::Balanced}};
        pilot.step(own, enemy);
        own.weapons.enemy_in_tr = true;   // koşullar World::update'ten gelir; bu test yalnız otopilot akışını sürer
        pilot.step(own, enemy);
        require(own.weapons.missiles_left() == 4 && own.decision.empty(), "destroy does not fire at TR");
        own.picture.is_in_dor = true;     // İmha: K1 DOR'da atar
        pilot.step(own, enemy);
        require(own.phase == Phase::Commit && own.weapons.missiles_left() == 4,
                "transition selects next state without flying it yet");
        pilot.step(own, enemy);
        require(own.weapons.missiles_left() == 3, "first shot once");
        own.weapons.missiles[0].python_id = "blue/missile/0";
        own.weapons.has_launch_order = false;

        own.picture.is_in_dor = false;    // K3'ü ayrı sürmek için
        own.picture.is_in_mar = true;
        pilot.step(own, enemy);
        pilot.step(own, enemy);
        require(own.phase == Phase::Crank, "first support exits on DOR, not MAR");
        own.picture.is_in_dor = true;
        own.picture.is_in_mar = false;
        pilot.step(own, enemy);
        own.picture.ata_deg = 150;
        pilot.step(own, enemy);
        require(own.phase == Phase::Out, "first escape starts");
        near(own.command.altitude_ft(), 26700, "first escape entry altitude");
        own.picture.ata_deg = 180;
        pilot.step(own, enemy);
        own.picture.ata_deg = own.profile.recommit_fire_ata_deg + 1.1;
        pilot.step(own, enemy);
        pilot.step(own, enemy);
        require(own.phase == Phase::Recommit && own.weapons.missiles_left() == 3,
                "recommit waits until target enters configured firing angle");
        own.picture.ata_deg = -own.profile.recommit_fire_ata_deg;
        for (int step = 0; step < 3; ++step)   // Recommit biter → Commit'te K1 (DOR içi) → atış
        {
            pilot.step(own, enemy);
        }
        require(own.weapons.missiles_left() == 2 && own.decision.rfind("K1", 0) == 0, "after recommit K1 decides the next shot");
        own.weapons.missiles[1].python_id = "blue/missile/1";
        own.weapons.has_launch_order = false;

        own.weapons.missiles[2].is_fired = own.weapons.missiles[3].is_fired = true;   // füze bitti: K5 ayrılır
        pilot.step(own, enemy);
        own.kinematics.altitude_ft = 26000;
        own.picture.ata_deg = 150;
        pilot.step(own, enemy);
        require(own.phase == Phase::Out, "support exits on DOR");
        near(own.command.altitude_ft(), 22700, "escape enters with current altitude");
        own.picture.ata_deg = 180;
        pilot.step(own, enemy);
        pilot.step(own, enemy);
        require(own.phase == Phase::Hold && own.weapons.missiles_left() == 0, "departure when no missile left");

        own.picture.ata_deg = 3;
        require(condition::nose_on_target(own), "positive alignment boundary");
        own.picture.ata_deg = -3;
        require(condition::nose_on_target(own), "negative alignment boundary");
        own.picture.ata_deg = -3.1;
        require(!condition::nose_on_target(own), "outside negative alignment boundary");
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
        hostile.kinematics.tas_kt = 300;
        hostile.kinematics.velocity_ned_kt.north = -300;

        const double range_nm = geometry::range_3d_nm(fighter.kinematics, hostile.kinematics);
        const double enemy_closure_kt = geometry::other_closure_kt(fighter.kinematics, hostile.kinematics);

        // WEZ (Rmax): 10 km'de füzenin kendi yolu + düşmanın uçuş süresince yaklaştığı yol; atış hızı 700 kt.
        const Wez wez = engagement::wez_nm(fighter, hostile);
        const RmaxFit& r_max = AIM120C_R_MAX;
        const double missile_path_nm = (r_max.a + r_max.b * std::log(10.0)) * (1.0 + r_max.k * (700.0 - r_max.launch_ref_kt));
        const double flight_time_s = r_max.c + r_max.d * std::log(10.0);
        near(wez.max_nm, missile_path_nm + enemy_closure_kt * flight_time_s / 3600.0, "Rmax formula", 1e-9);
        require(wez.max_nm > missile_path_nm, "WEZ longer when the enemy approaches");
        near((r_max.a + r_max.b * std::log(10.0)), 60.65, "Rmax missile path at 10 km and reference speed", 1e-3);
        require(engagement::in_wez(fighter, hostile) == (range_nm > wez.min_nm && range_nm < wez.max_nm), "in WEZ");

        // R_cold: 10 km ve 700 kt'de safety · e^a
        near(engagement::r_cold_nm(fighter, hostile),
             AIM120C_R_COLD.safety * std::exp(AIM120C_R_COLD.a), "R_cold at reference point");

        // NEZ = R_cold + yaklaşma + dönerken kaçamadığım yol; MAR bunun üstüne patlama yarıçapı ve out payı ekler.
        const OutPlan out = engagement::out_plan(fighter, hostile);
        const double nez_nm = engagement::nez_nm(fighter, hostile);
        near(nez_nm, engagement::r_cold_nm(fighter, hostile) + out.approach_nm
                     + travel_distance_nm(out.escape_kt, out.turn.time_s), "NEZ");
        require(engagement::mar_nm(fighter, hostile)
                >= nez_nm + feet_to_nmile(AIM120C.blast_radius_ft) + fighter.profile.out_offset_nm, "MAR covers NEZ");

        // DOR = MAR + hedef atış açısına recommit sırasında düşmanın TAS'ıyla yaklaşması + 0,5 NM.
        const double recommit_turn_rad = (180 - fighter.profile.recommit_fire_ata_deg) * DEG_TO_RAD;
        const double recommit_turn_s = recommit_turn_rad * turn::radius_m(700, fighter.profile.out_g) / knot_to_mps(700);
        const double remaining_nm = engagement::dor_nm(fighter, hostile)
                                    - travel_distance_nm(hostile.kinematics.tas_kt, recommit_turn_s);
        near(remaining_nm, engagement::mar_nm(fighter, hostile) + 0.5,
             "DOR leaves MAR plus fixed 0.5 NM margin");


    }

    void decision_range()
    {
        Aircraft own, enemy;
        own.profile = aircraft_profile("F16");
        enemy.profile = aircraft_profile("F16");
        own.kinematics.lat_deg = 59;
        own.kinematics.lon_deg = 18;
        own.kinematics.altitude_ft = meter_to_feet(10000);
        own.kinematics.tas_kt = 700;
        own.kinematics.velocity_ned_kt.north = 700;
        enemy.kinematics = place(own.kinematics, 0, 20);
        enemy.kinematics.tas_kt = 600;
        enemy.kinematics.velocity_ned_kt.north = -600;
        own.profile.out_offset_nm = 0.75;

        const double stern_wez_nm = engagement::r_cold_nm(own, enemy);
        near(own.profile.dr_notch_time_s, 15, "DR default notch duration");
        // 600 kt, 15 s: düşman 2.5 NM yaklaşır; kendi 700 kt hızımız eklenmez.
        const double approaching_dr_nm = engagement::dr_nm(own, enemy);
        near(approaching_dr_nm - stern_wez_nm, 3.0, "DR enemy TAS and fixed 0.5 NM margin");
        own.kinematics.velocity_ned_kt = {0, 700, 0};
        near(engagement::dr_nm(own, enemy), approaching_dr_nm, "DR excludes own closure component");

        // DR, düşmanın yönünden bağımsız TAS'ını kullanır; yaklaşma için en kötü durum kabul edilir.
        enemy.kinematics.velocity_ned_kt = {0, 600, 0};
        near(engagement::dr_nm(own, enemy), approaching_dr_nm, "DR uses TAS for crossing target");
        enemy.kinematics.velocity_ned_kt = {600, 0, 0};
        near(engagement::dr_nm(own, enemy), approaching_dr_nm, "DR uses TAS for receding target");
        enemy.kinematics.velocity_ned_kt = {-600, 0, 0};
        own.profile.dr_notch_time_s = 30;
        near(engagement::dr_nm(own, enemy) - stern_wez_nm, 5.5, "DR configurable notch duration");
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
            missile.target.a_pole_nm = 20.0 + slot;
            missile.target.f_pole_nm = 10.0 + slot;
            world.hostile.weapons.missiles.push_back(missile);
        }

        world.update();
        require(world.fighter.enemy_id == "red", "enemy assigned");
        const std::vector<const Missile*>& threats = world.fighter.picture.threats;
        require(threats.size() == 2, "every inbound missile is scanned");
        require(threats[0] == &world.hostile.weapons.missiles[0], "threat is the missile itself, not a copy");
        require(world.fighter.picture.threat != nullptr, "threat found");
        require(world.fighter.picture.threat->slot == 1, "closest missile is the threat");
        near(geometry::bearing_deg(world.fighter.kinematics, world.fighter.picture.threat->kinematics), 0, "threat bearing", 1e-6);
        near(*threats[0]->target.a_pole_nm, 20, "A-pole read from missile");
        near(*threats[1]->target.f_pole_nm, 11, "F-pole read from missile");
        require(threats[1]->target.catch_time_left_s
                == missile_geometry::catch_time_left_s(*threats[1]), "catch time computed for every missile");
        require(world.fighter.picture.is_spiked, "spiked while a missile is inbound");
        require(world.hostile.picture.threats.empty(), "no threat list against hostile");
        require(world.hostile.picture.threat == nullptr, "no missile against hostile");
        require(!world.hostile.picture.is_spiked, "not spiked without missile");
        // Silah ve kaçış hesapları main_weapon'dan; füze slotu olmasa da hesaplanır.
        require(world.hostile.picture.mar_nm > 0, "MAR from main weapon without missile slots");
        near(world.fighter.picture.dr_nm, engagement::dr_nm(world.fighter, world.hostile), "fighter DR computed");
        near(world.hostile.picture.dr_nm, engagement::dr_nm(world.hostile, world.fighter), "hostile DR computed");
        near(world.fighter.weapons.wez.min_nm, AIM120C.min_range_nm, "WEZ from main weapon without missile slots");

        // hostile'ın NEZ'i = fighter'ın kendi kaçış hesabındaki NEZ (aynı füze, aynı out planı); simetri.
        near(world.hostile.weapons.nez_nm, world.fighter.picture.nez_nm, "weapon NEZ mirrors enemy escape NEZ", 1e-9);
        near(world.fighter.weapons.nez_nm, world.hostile.picture.nez_nm, "weapon NEZ mirrors enemy escape NEZ (other side)", 1e-9);
        require(world.hostile.weapons.f_pole_nm == engagement::f_pole_nm(world.hostile, world.fighter), "F-pole computed");
        require(world.fighter.weapons.a_pole_nm == engagement::a_pole_nm(world.fighter, world.hostile), "A-pole computed");
        require(world.fighter.weapons.tr_nm == engagement::tr_nm(world.fighter, world.hostile), "TR computed");
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
        world.fighter.picture.dr_nm = 23.25;
        world.fighter.weapons.a_pole_nm = 18;
        world.fighter.weapons.f_pole_nm = 15;
        world.fighter.weapons.tr_nm = 30;
        Missile threat;
        threat.target.range_nm = 9;
        threat.kinematics.mach = 2.5;
        threat.target.is_pitbull = true;
        world.fighter.picture.threat = &threat;
        world.fighter.command.set_g_limit(6.0, 9.0);
        const Message message = CommandWriter::write(1, world);
        const Json& fighter_command = message.data.at("aircraft").at(0);
        near(fighter_command.at("altitude_m").get<double>(), 3048, "writer metric command");
        near(fighter_command.at("heading_deg").get<double>(), 270, "writer heading 0..360");
        near(fighter_command.at("g_limit").get<double>(), 6.0, "writer g limit");
        require(!message.data.at("aircraft").at(1).contains("g_limit"), "no g limit when unset");
        const Json& calc = message.data.at("calculations").at("aircraft").at(0);
        near(calc.at("range_3d_nm").get<double>(), 12, "calculation NM field");
        near(calc.at("rate_of_closure_kt").get<double>(), 500, "calculation kt field (wire name)");
        near(calc.at("dr_nm").get<double>(), 23.25, "DR exported in NM");
        near(calc.at("a_pole_nm").get<double>(), 18, "A-pole exported in NM");
        near(calc.at("f_pole_nm").get<double>(), 15, "F-pole exported in NM");
        near(calc.at("tr_nm").get<double>(), 30, "TR exported in NM");
        const Json& empty_calc = message.data.at("calculations").at("aircraft").at(1);
        near(empty_calc.at("a_pole_nm").get<double>(), 0, "absent A-pole converted only at wire boundary");
        near(empty_calc.at("f_pole_nm").get<double>(), 0, "absent F-pole converted only at wire boundary");
        near(empty_calc.at("tr_nm").get<double>(), 0, "absent TR converted only at wire boundary");
        near(calc.at("threat_range_nm").get<double>(), 9, "threat range exported in NM");
        near(calc.at("threat_mach").get<double>(), 2.5, "threat mach exported");
        require(calc.at("threat_pitbull").get<int>() == 1, "threat pitbull exported as number");
        near(empty_calc.at("threat_range_nm").get<double>(), 0, "absent threat exported as 0");
        near(empty_calc.at("threat_mach").get<double>(), 0, "absent threat mach exported as 0");
        require(empty_calc.at("threat_pitbull").get<int>() == 0, "absent threat not pitbull");
        require(!calc.contains("range_3d_m"), "no metric calculation label for NM value");

        require(world.fighter.weapons.fire(AIM120C), "launch test missile");
        require(world.fighter.weapons.has_launch_order, "launch order set");
        data["missiles"][0] = {{"id", "blue/missile/0"}, {"owner_id", "blue"},
            {"target_id", "red"}, {"state", "in_flight"}, {"outcome", nullptr},
            {"kinematics", data["aircraft"][0]}};
        StateReader::read_world(data, world);
        require(world.fighter.weapons.has_launch_order, "reader leaves launch order reset to Sim::step");
        const Missile& missile = world.fighter.weapons.missiles[0];
        require(missile.has_kinematics, "missile kinematics matched");
        require(world.fighter.weapons.active_missile() == &missile, "active missile");
        near(missile.kinematics.altitude_ft, 10000, "missile reader altitude");
        near(missile.kinematics.tas_kt, mps_to_knot(300), "missile reader TAS");
        world.fighter.weapons.missiles[0].target.is_pitbull = true;
        const Json pitbull_calc = CommandWriter::write(2, world).data.at("calculations").at("aircraft");
        require(pitbull_calc[0].at("pitbull_missile_slots").get<std::string>() == "[0]", "pitbull slot exported");
        require(pitbull_calc[1].at("pitbull_missile_slots").get<std::string>() == "[]", "pitbull belongs to carrier");
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
        maneuver_lifecycle();
        otopilot_flow();
        engagement_values();
        decision_range();
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
