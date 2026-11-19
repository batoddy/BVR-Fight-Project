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
#include "control/legacy/otopilot.hpp"
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

    Aircraft f16()
    {
        Aircraft aircraft;
        aircraft.profile = aircraft_profile("F16");
        return aircraft;
    }

    template <class Ex = std::runtime_error, class F> bool rejects(F&& call)
    {
        try { call(); }
        catch (const Ex&) { return true; }
        return false;
    }

    bool said(const Aircraft& own, const char* prefix) { return own.decision.rfind(prefix, 0) == 0; }

    void near_fields(const Json& json, std::initializer_list<std::pair<const char*, double>> fields)
    {
        for (const auto& [key, value] : fields)
        {
            near(json.at(key).get<double>(), value, key);
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
        const std::pair<double, double> pairs[] = {{meter_to_feet(3048), 10000}, {feet_to_meter(10000), 3048},
            {meter_to_nmile(1852), 1}, {nmile_to_meter(1), 1852}, {knot_to_mps(3600), 1852}, {mps_to_knot(-1852), -3600},
            {feet_to_nmile(nmile_to_feet(1)), 1}, {fps_to_knot(knot_to_fps(450)), 450}};
        for (const auto& [actual, expected] : pairs)
        {
            near(actual, expected, "unit conversion");
        }
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
        Aircraft fighter = f16(), hostile;
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

    // Otopilot akışı: koşullar World::update'ten gelir; burada picture ve weapons elle sürülür.
    void otopilot_flow()
    {
        Aircraft own = f16(), enemy;
        own.kinematics.altitude_ft = 30000;
        own.kinematics.tas_kt = 500;
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
        pilot.step(own);
        require(own.phase == Phase::Commit && own.weapons.missiles_left() == 4, "commit waits outside WEZ");

        own.weapons.enemy_in_wez = true;
        own.weapons.enemy_in_tr = true;
        own.weapons.tr_nm = own.picture.range_3d_nm;   // World'deki gibi: bayrak ve değer birlikte
        pilot.step(own);
        require(said(own, "COMMIT: ateş") && own.weapons.missiles_left() == 4, "fire decided at TR, order next step");
        pilot.step(own);
        require(own.weapons.missiles_left() == 3, "first shot once");
        Missile& first = own.weapons.missiles[0];
        own.weapons.has_launch_order = false;   // Sim::step gibi: emir gitti, sim slot 0'ı attı
        first.has_kinematics = true;
        first.state = MissileState::InFlight;
        own.weapons.record_launch(first);
        pilot.step(own);
        require(own.phase == Phase::Crank && first.fire_order == 1, "support cranks against hot enemy");

        first.target.is_pitbull = true;
        pilot.step(own);
        require(said(own, "SUPPORT → OUT"), "support ends at pitbull");
        pilot.step(own);
        require(own.phase == Phase::Out, "out flown");
        near(own.command.altitude_ft(), 26700, "out entry altitude");
        require(own.command.throttle() == throttle::MAX_THRUST, "out on afterburner");

        own.picture.ata_deg = 180;
        pilot.step(own);
        require(said(own, "OUT → COLD"), "out done goes cold");
        pilot.step(own);
        require(said(own, "COLD → RECOMMIT"), "hot enemy beyond MRR: recommit");
        own.picture.ata_deg = 0;
        pilot.step(own);
        require(said(own, "RECOMMIT → COMMIT"), "recommit done");

        // Tehdit gelirken füzem pitbull değil: TRADE.
        Missile threat;
        threat.profile = AIM120C;
        threat.has_kinematics = true;
        threat.state = MissileState::InFlight;
        threat.target.catch_time_left_s = 60.0;
        threat.target.pitbull_time_left_s = 40.0;
        own.picture.threat = &threat;
        own.picture.out.turn.time_s = 15;
        first.target.is_pitbull = false;
        first.target.pitbull_time_left_s = 20.0;
        pilot.step(own);
        require(said(own, "TEHDİT → TRADE") && own.phase == Phase::Crank, "threat while supporting: trade");
        pilot.step(own);
        require(said(own, "TEHDİT → TRADE"), "A holds: crank continues");

        first.target.pitbull_time_left_s = 50.0;          // kritik an 60 − (15 + 10) = 35 s'den sonra
        first.target.catch_time_left_s.reset();           // B2/B1 hesaplanamaz
        own.picture.threat_escape_margin_nm = 10.0;
        pilot.step(own);
        require(own.phase == Phase::Crank && said(own, "TEHDİT → TRADE"), "A fails: crank until critical moment");
        threat.target.catch_time_left_s = 25.0;           // kritik an geçti: 25 − (15 + 10) = 0
        pilot.step(own);
        require(said(own, "DEFEND"), "critical moment: drop support");
        threat.target.catch_time_left_s = 60.0;
        pilot.step(own);
        require(own.phase == Phase::Beam, "large escape margin: beam");
        own.picture.threat_escape_margin_nm = -1.0;
        own.picture.notch_turn_s = 10;
        pilot.step(own);
        require(said(own, "DEFEND: acilleşti"), "negative margin escalates");
        pilot.step(own);
        require(own.phase == Phase::Drag, "drag before last moment");
        threat.target.catch_time_left_s = 12.0;
        pilot.step(own);
        pilot.step(own);
        require(own.phase == Phase::Notch, "notch at last moment");
        threat.target.catch_time_left_s = 60.0;
        own.picture.threat_escape_margin_nm = 10.0;
        pilot.step(own);
        require(own.phase == Phase::Notch, "defense never de-escalates");

        own.picture.threat = nullptr;
        own.picture.threat_escape_margin_nm.reset();
        pilot.step(own);
        require(said(own, "REASSESS → RECOMMIT"), "threat over: reassess");

        // Zararsız füze: benden yavaş ve recommit payı ≥ 1 NM → savunma bırakılır, kesinti de tetiklenmez.
        first.state = MissileState::Ended;                // füzem bitti: TRADE yok
        own.weapons.enemy_in_tr = false;                  // karşı atış yok
        threat.kinematics.tas_kt = 1500;
        own.picture.threat = &threat;
        own.picture.threat_recommit_margin_nm = 2.0;
        pilot.step(own);
        require(said(own, "DEFEND"), "fast threat: defend");
        threat.kinematics.tas_kt = 400;                   // ben 500 kt
        pilot.step(own);
        require(!said(own, "DEFEND: ") && own.decision.find("REASSESS") != std::string::npos,
                "harmless threat: leave defense");
        pilot.step(own);
        require(!said(own, "DEFEND"), "harmless threat does not interrupt");
        own.picture.threat_recommit_margin_nm = 0.5;      // füzeye yaklaştım: pay eşiğin altında
        pilot.step(own);
        require(said(own, "DEFEND"), "margin below threshold: threat again");
        own.picture.threat = nullptr;
        own.picture.threat_recommit_margin_nm.reset();

        for (Missile& missile : own.weapons.missiles)   // füze bitti: kalıcı çıkış
        {
            missile.has_kinematics = true;
            missile.state = MissileState::Ended;
        }
        pilot.step(own);
        require(said(own, "ABORT") && own.phase == Phase::Out, "abort when out of missiles");
        own.picture.range_3d_nm = 50;
        pilot.step(own);
        require(own.phase == Phase::Out, "abort is permanent");
    }

    // Erken atış payı: TR'nin dışında, TR + pay içinde ateş; pay 0 ise TR beklenir.
    void commit_early_shot()
    {
        Aircraft own = f16();
        Missile missile;
        missile.profile = AIM120C;
        own.weapons.missiles.push_back(missile);
        own.weapons.enemy_in_wez = true;
        own.weapons.tr_nm = 20;
        own.picture.range_3d_nm = 22.5;

        const Mission safe_early{MissionGoal::Destroy, RiskLevel::Safe, false, 3.0};
        const Mission on_tr{MissionGoal::Destroy, RiskLevel::Safe};
        require(decide::commit(own, safe_early).choice == CommitChoice::Fire, "early shot inside TR + margin");
        require(decide::commit(own, on_tr).choice == CommitChoice::Continue, "no margin waits for TR");
        own.picture.range_3d_nm = 23.5;
        require(decide::commit(own, safe_early).choice == CommitChoice::Continue, "outside TR + margin waits");
    }

    void missile_decision_metrics()
    {
        Missile missile;
        missile.profile = AIM120C;
        missile.target.range_nm = 20;
        missile.target.closure_kt = 2000;
        near(*missile_geometry::pitbull_time_left_s(missile),
             *accel_travel_time_s(20 - AIM120C.pitbull_range_nm, 2000, AIM120C.slowing_accel_kt_s), "pitbull time left");
        missile.target.is_pitbull = true;
        near(*missile_geometry::pitbull_time_left_s(missile), 0, "pitbull time is zero once pitbull");

        // Füze 2000 kt, Mach 1 = 590 kt, kapanma 2270 kt; 15 s dönüş, 600 kt kaçış.
        missile.kinematics.tas_kt = 2000;
        missile.kinematics.mach = 2000.0 / 590.0;
        missile.target.closure_kt = 2270;
        missile.target.range_nm = 15;
        const double accel = AIM120C.slowing_accel_kt_s;
        const double speed_after_turn_kt = 2000 + accel * 15;
        const double closure_after_turn_kt = speed_after_turn_kt - 600;
        const double turn_closed_nm = (2270 + closure_after_turn_kt) / 2 * 15 / 3600;
        const double time_to_end_s = (speed_after_turn_kt - 600) / -accel;
        const double run_closed_nm = (closure_after_turn_kt * time_to_end_s + accel * time_to_end_s * time_to_end_s / 2) / 3600;
        const double expected_nm = 15 - feet_to_nmile(AIM120C.blast_radius_ft) - turn_closed_nm - run_closed_nm;
        near(missile_geometry::escape_margin_nm(missile, 15, 600), expected_nm, "escape margin (drag)", 1e-9);
        require(expected_nm < 0, "15 NM: cannot escape");
        missile.target.range_nm = 25;
        require(missile_geometry::escape_margin_nm(missile, 15, 600) > 0, "25 NM: escape");

        const double recommit_end_s = (speed_after_turn_kt - 590) / -accel;   // füzeye dönünce füze Mach 1'de ölür
        const double recommit_closure_kt = speed_after_turn_kt + 500;
        const double recommit_turn_nm = (2270 + recommit_closure_kt) / 2 * 15 / 3600;
        const double recommit_run_nm = (recommit_closure_kt * recommit_end_s + accel * recommit_end_s * recommit_end_s / 2) / 3600;
        near(missile_geometry::escape_margin_nm(missile, 15, -500),
             25 - feet_to_nmile(AIM120C.blast_radius_ft) - recommit_turn_nm - recommit_run_nm, "escape margin (recommit)", 1e-9);
    }

    void engagement_values()
    {
        // İki F16 (ana silah AIM-120C): fighter 10 km'de 700 kt kuzeye, hostile 20 NM kuzeyde bana doğru 300 kt.
        Aircraft fighter = f16(), hostile = f16();
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
        near((r_max.a + r_max.b * std::log(10.0)), 31.223, "Rmax missile path at 10 km and reference speed", 1e-3);
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
        Aircraft own = f16(), enemy = f16();
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
            missile.has_kinematics = true;
            missile.state = MissileState::InFlight;
            missile.target_id = "blue";
            missile.kinematics = place(world.fighter.kinematics, 0, slot == 0 ? 12 : 6);
            missile.kinematics.velocity_ned_kt.north = -1500;
            missile.kinematics.tas_kt = 1500;
            missile.kinematics.mach = 2.5;
            missile.target.a_pole_nm = 20.0 + slot;
            missile.target.f_pole_nm = 10.0 + slot;
            world.hostile.weapons.missiles.push_back(missile);
        }

        world.update();
        require(world.fighter.enemy_id == "red", "enemy assigned");
        const std::vector<const Missile*>& threats = world.fighter.picture.threats;
        require(threats.size() == 2, "every inbound missile is scanned");
        require(threats[0] == &world.hostile.weapons.missiles[0], "threat is the missile itself, not a copy");
        require(world.fighter.picture.threat->slot == 1, "closest missile is the threat");
        near(geometry::bearing_deg(world.fighter.kinematics, world.fighter.picture.threat->kinematics), 0, "threat bearing", 1e-6);
        near(*threats[0]->target.a_pole_nm, 20, "A-pole read from missile");
        near(*threats[1]->target.f_pole_nm, 11, "F-pole read from missile");
        require(threats[1]->target.catch_time_left_s
                == missile_geometry::catch_time_left_s(*threats[1]), "catch time computed for every missile");
        require(world.fighter.picture.is_spiked, "spiked while a missile is inbound");
        require(threats[1]->target.pitbull_time_left_s == missile_geometry::pitbull_time_left_s(*threats[1]), "pitbull time computed");
        require(world.fighter.picture.threat_escape_margin_nm.has_value()
                && world.fighter.picture.threat_recommit_margin_nm.has_value(), "threat margins computed");
        require(!world.hostile.picture.threat_escape_margin_nm.has_value(), "no margin without threat");
        near(world.fighter.picture.mrr_nm, engagement::mrr_nm(world.fighter, world.hostile), "MRR computed");
        near(world.fighter.weapons.r_cold_nm, engagement::r_cold_nm(world.hostile, world.fighter), "my R_cold mirrors enemy escape R_cold");
        require(world.hostile.picture.threats.empty(), "no threat list against hostile");
        require(!world.hostile.picture.is_spiked, "not spiked without missile");
        // Silah ve kaçış hesapları main_weapon'dan; füze slotu olmasa da hesaplanır.
        require(world.hostile.picture.mar_nm > 0, "MAR from main weapon without missile slots");
        near(world.fighter.picture.dr_nm, engagement::dr_nm(world.fighter, world.hostile), "fighter DR computed");
        near(world.hostile.picture.dr_nm, engagement::dr_nm(world.hostile, world.fighter), "hostile DR computed");
        near(world.fighter.weapons.wez.min_nm, AIM120C.min_range_nm, "WEZ from main weapon without missile slots");

        // hostile'ın NEZ'i = fighter'ın kendi kaçış hesabındaki NEZ (aynı füze, aynı out planı); simetri.
        require(world.hostile.weapons.f_pole_nm == engagement::f_pole_nm(world.hostile, world.fighter), "F-pole computed");
        require(world.fighter.weapons.a_pole_nm == engagement::a_pole_nm(world.fighter, world.hostile), "A-pole computed");
        require(world.fighter.weapons.tr_nm == engagement::tr_nm(world.fighter, world.hostile), "TR computed");
        require(world.fighter.picture.me_in_enemy_wez == world.hostile.weapons.enemy_in_wez, "me_in_enemy_wez copied");
        const double range_nm = world.hostile.picture.range_3d_nm;
        require(world.hostile.weapons.enemy_in_nez == (range_nm <= world.fighter.picture.nez_nm), "enemy in NEZ");

        world.hostile.weapons.missiles[1].target.is_guided = false;   // sim is_locked false
        world.update();
        require(!world.hostile.weapons.missiles[1].target.is_guided, "world keeps sim lock flag");
        require(world.fighter.picture.threats.size() == 2, "unguided missile still listed");
    }

    // Protokol v2 örnekleri: docs/mimari/MESAJ_PROTOKOLU_V2.md
    Json wire_kinematics()
    {
        return {{"lat_deg", 59}, {"lon_deg", 18}, {"alt_m", 3048}, {"yaw_deg", 270}, {"pitch_deg", 1}, {"roll_deg", 2},
                {"tas_mps", 300}, {"mach", 0.9}, {"velocity_ned_mps", {200, -30, 4}}};
    }

    Json hello_message(const std::string& slot_1_designation)
    {
        Json data;
        data["physics_dt_s"] = 1.0 / 120;
        data["tick_s"] = 1.0 / 12;
        data["aircraft"] = Json::array();
        for (const std::string id : {"blue", "red"})
        {
            data["aircraft"].push_back({{"id", id}, {"name", id}, {"type", "F16"}, {"team", id},
                {"weapons", {{{"slot", 0}, {"profile", "aim9x"}, {"designation", "AIM-9X"}},
                             {{"slot", 1}, {"profile", "aim120c5"}, {"designation", slot_1_designation}}}}});
        }
        return data;
    }

    Json ready_weapon(int slot)
    {
        return {{"slot", slot}, {"id", "x/missile/" + std::to_string(slot)}, {"designation", "AIM-120C"},
                {"state", "ready"}, {"request_id", nullptr}, {"target_id", nullptr}, {"flight_time_s", nullptr},
                {"kinematics", nullptr}, {"guidance_mode", "none"}, {"is_locked", false}, {"is_pitbull", false},
                {"outcome", nullptr}};
    }

    Json state_message()
    {
        Json data;
        data["time_s"] = 1.0;
        data["aircraft"] = Json::object();
        for (const std::string id : {"blue", "red"})
        {
            Json packet;
            packet["own"] = {{"id", id}, {"name", id}, {"type", "F16"}, {"team", id}, {"health", 1},
                {"kinematics", wire_kinematics()}, {"load_factor_g", 1.0}, {"fuel_kg", 3000.0},
                {"applied_command", {{"heading_deg", 270}, {"altitude_m", 3048}, {"throttle", 0.7}, {"g_limit", nullptr}}}};
            packet["weapons"] = Json::array({ready_weapon(0), ready_weapon(1)});
            packet["warnings"] = Json::array();
            packet["events"] = Json::array();
            data["aircraft"][id] = packet;
        }
        return data;
    }

    void protocol_v2_reader_and_writer()
    {
        World unknown;
        require(rejects([&] { StateReader::read_hello(hello_message("AIM-999"), unknown); }), "unknown designation is an error");

        World world;
        StateReader::read_hello(hello_message("AIM-120C"), world);
        require(world.fighter.weapons.missiles.size() == 2, "slots from hello");
        require(world.fighter.weapons.missiles[0].profile.name == "AIM-9X"
                && world.fighter.weapons.missiles[1].profile.name == "AIM-120C", "loadout from hello designations");
        require(world.hostile.profile.name == "F-16", "profile from hello type");

        Json data = state_message();
        StateReader::read_world(data, world);
        const Kinematics& read = world.fighter.kinematics;
        near(world.sim_time_s, 1.0, "reader time_s");
        near(read.altitude_ft, 10000, "reader altitude");
        near(read.heading_deg, -90, "reader heading ±180");
        near(read.tas_kt, mps_to_knot(300), "reader TAS");
        near(read.velocity_ned_kt.down, mps_to_knot(4), "reader vertical speed");
        near(read.velocity_ned_kt.east, mps_to_knot(-30), "reader signed speed");
        near(world.fighter.command.altitude_ft(), 10000, "reader current command");
        near(world.fighter.command.heading_deg(), -90, "reader command heading ±180");
        require(!world.fighter.command.g_limit().has_value(), "reader leaves g limit to maneuver");
        require(world.fighter.command.mode() == CommandMode::Target, "applied command without mode is target");

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
        require(fighter_command.at("mode").get<std::string>() == "target", "writer target mode");
        near_fields(fighter_command, {{"altitude_m", 3048}, {"heading_deg", 270}, {"g_limit", 6.0}});
        require(!message.data.at("aircraft").at(1).contains("g_limit"), "no g limit when unset");
        const Json& calc = message.data.at("calculations").at("aircraft").at(0);
        near_fields(calc, {{"range_3d_nm", 12}, {"rate_of_closure_kt", 500}, {"dr_nm", 23.25}, {"a_pole_nm", 18}, {"f_pole_nm", 15},
                           {"tr_nm", 30}, {"threat_range_nm", 9}, {"threat_mach", 2.5}, {"threat_pitbull", 1}});
        const Json& empty_calc = message.data.at("calculations").at("aircraft").at(1);   // yokluk yalnız tel sınırında 0'a çevrilir
        near_fields(empty_calc, {{"a_pole_nm", 0}, {"f_pole_nm", 0}, {"tr_nm", 0}, {"threat_range_nm", 0}, {"threat_mach", 0}, {"threat_pitbull", 0}});
        require(!calc.contains("range_3d_m"), "no metric calculation label for NM value");

        require(calc.at("phase").get<std::string>() == "HOLD", "phase exported by name");
        world.fighter.phase = Phase::PurePursuit;
        require(CommandWriter::write(1, world).data.at("calculations").at("aircraft").at(0).at("phase").get<std::string>()
                == "PURE_PURSUIT", "multi-word phase name");

        // Atış emri: hangi slotun atılacağını sim seçer; command'a yalnız atan ve hedef gider.
        require(world.fighter.weapons.fire(), "launch test missile");
        require(world.fighter.weapons.has_launch_order, "launch order set");
        require(world.fighter.weapons.launch_pending() && world.fighter.weapons.active_missile() == nullptr,
                "ordered missile pending, not active");
        require(world.fighter.weapons.missiles_left() == 1, "order counted before sim launches");
        require(!world.fighter.weapons.fire(), "one order per step");
        const Json launch = CommandWriter::write(3, world).data.at("launch");
        require(launch.size() == 1 && !launch[0].contains("request_id")
                && launch[0].at("id").get<std::string>() == "blue" && launch[0].at("target_id").get<std::string>() == "red",
                "launch carries shooter and target");

        // Sim ilk hazır slotu (0) attı: atış sırası ve pole'lar sim'in gösterdiği slota yazılır.
        world.fighter.weapons.has_launch_order = false;   // Sim::step her step sıfırlar
        Json flying = ready_weapon(0);
        flying["state"] = "in_flight";
        flying["target_id"] = "red";
        flying["flight_time_s"] = 0.1;
        flying["kinematics"] = wire_kinematics();
        flying["guidance_mode"] = "datalink";
        flying["is_locked"] = true;
        data["aircraft"]["blue"]["weapons"][0] = flying;
        StateReader::read_world(data, world);
        const Missile& missile = world.fighter.weapons.missiles[0];
        require(missile.has_kinematics && missile.fire_order == 1, "launch recorded on sim slot");
        require(missile.target.a_pole_nm == 18.0 && missile.target.f_pole_nm == 15.0, "poles at launch copied");
        require(missile.guidance_mode == GuidanceMode::Datalink && missile.target.is_guided && !missile.target.is_pitbull, "lock from sim");
        require(world.fighter.weapons.active_missile() == &missile && !world.fighter.weapons.launch_pending(), "active missile");
        near(missile.kinematics.altitude_ft, 10000, "missile reader altitude");
        StateReader::read_world(data, world);
        require(missile.fire_order == 1, "launch recorded once");

        data["aircraft"]["blue"]["weapons"][0]["is_pitbull"] = true;
        data["aircraft"]["blue"]["weapons"][0]["guidance_mode"] = "seeker";
        StateReader::read_world(data, world);
        const Json pitbull_calc = CommandWriter::write(4, world).data.at("calculations").at("aircraft");
        require(pitbull_calc[0].at("pitbull_missile_slots").get<std::string>() == "[0]", "pitbull slot exported");
        require(pitbull_calc[1].at("pitbull_missile_slots").get<std::string>() == "[]", "pitbull belongs to carrier");
        data["aircraft"]["blue"]["weapons"][0]["is_locked"] = false;
        data["aircraft"]["blue"]["weapons"][0]["guidance_mode"] = "none";
        StateReader::read_world(data, world);
        require(world.fighter.weapons.active_missile() == nullptr, "unlocked missile not active");
    }

    // Doğrudan mod: setter sınırları, okuma, yazım.
    void protocol_v21_direct_mode()
    {
        FlightCommand command;
        command.set_heading_deg(90).set_altitude_ft(20000).set_throttle(0.5).set_g_limit(5.0, 9.0);
        require(command.mode() == CommandMode::Target, "default target mode");
        command.set_bank_deg(190).set_g(12);
        require(command.mode() == CommandMode::Direct, "bank and g switch to direct");
        near(command.bank_deg(), -170, "bank wrapped to ±180");
        near(command.g(), DIRECT_G_MAX, "g clamped to +9");
        near(command.set_g(-6).g(), DIRECT_G_MIN, "g clamped to -4");

        World world;
        StateReader::read_hello(hello_message("AIM-120C"), world);
        Json data = state_message();
        data["aircraft"]["blue"]["own"]["applied_command"]["mode"] = "direct";
        data["aircraft"]["blue"]["own"]["applied_command"]["bank_deg"] = -30.0;
        data["aircraft"]["blue"]["own"]["applied_command"]["g"] = 2.5;
        StateReader::read_world(data, world);
        require(world.fighter.command.mode() == CommandMode::Direct, "applied direct mode read");
        near(world.fighter.command.bank_deg(), -30, "applied bank read");
        near(world.fighter.command.g(), 2.5, "applied g read");
        require(world.hostile.command.mode() == CommandMode::Target, "other aircraft stays target");

        world.fighter.command.set_bank_deg(60).set_g(-2).set_throttle(0.9);
        const Json aircraft = CommandWriter::write(2, world).data.at("aircraft");
        const Json& direct = aircraft.at(0);
        require(direct.at("mode").get<std::string>() == "direct" && direct.at("id").get<std::string>() == "blue", "writer direct mode");
        near_fields(direct, {{"bank_deg", 60}, {"g", -2}, {"throttle", 0.9}});
        require(direct.size() == 5 && !direct.contains("heading_deg") && !direct.contains("g_limit"), "direct writes only its fields");
        require(aircraft.at(1).at("mode").get<std::string>() == "target" && aircraft.at(1).contains("heading_deg"), "target fields kept");

        world.fighter.command.set_g(std::nan(""));
        require(rejects([&] { CommandWriter::write(3, world); }), "NaN direct g rejected");
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
        missile_decision_metrics();
        commit_early_shot();
        engagement_values();
        decision_range();
        world_threat_selection();
        protocol_v2_reader_and_writer();
        protocol_v21_direct_mode();
        std::cout << "Unit conversion, geometry, turn, engagement, world and IO checks passed.\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
