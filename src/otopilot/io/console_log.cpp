#include "io/console_log.hpp"

#include <iomanip>
#include <iostream>

#include "control/otopilot_maneuver_test.hpp"

namespace
{
    void print_aircraft(const Aircraft& aircraft)   // tek satırda uçağın durumu
    {
        const Kinematics& kinematics = aircraft.kinematics;
        std::cout << aircraft.id
                  << "  lat " << std::setprecision(4) << kinematics.lat_deg
                  << "  lon " << kinematics.lon_deg
                  << "  alt " << std::setprecision(0) << kinematics.altitude_ft << " ft"
                  << "  hdg " << std::setprecision(1) << kinematics.heading_deg
                  << "  tas " << std::setprecision(0) << kinematics.tas_kt << " kt"
                  << "  rng " << std::setprecision(1) << aircraft.picture.range_3d_nm << " NM"
                  << "  brg " << aircraft.picture.bearing_deg
                  << "  kalan " << aircraft.weapons.missiles_left() << '/' << aircraft.weapons.missiles.size();
    }

    void print_active_missiles(const Aircraft& own)   // havadaki füzelerim, atış sırasıyla: sıra, slot, hedefe uzaklık, pitbull
    {
        for (const Missile* missile : own.weapons.active_missiles())
        {
            std::cout << "  | fuze " << missile->fire_order << " (slot " << missile->slot << ")"
                      << "  hedefe " << std::setprecision(1) << missile->target.range_nm << " NM";
            if (missile->target.is_pitbull)
            {
                std::cout << "  pitbull";
            }
        }
    }
}

void ConsoleLog::print(const World& world)
{
    const int second = static_cast<int>(world.sim_time_s);
    if (second == last_print_s_)
    {
        return;
    }

    last_print_s_ = second;
    std::cout << std::fixed << "t " << std::setw(3) << second << " s | ";
    print_aircraft(world.fighter);
    std::cout << " | ";
    print_aircraft(world.hostile);
    print_active_missiles(world.fighter);
    std::cout << '\n';
}

void ConsoleLog::print_maneuver_test(const World& world, const OtopilotManeuverTest& pilot)
{
    const int second = static_cast<int>(world.sim_time_s);
    if (second == last_test_print_s_ && pilot.leg_index() == last_test_leg_)
    {
        return;
    }
    last_test_print_s_ = second;
    last_test_leg_ = pilot.leg_index();
    const char* status = "Running";
    switch (pilot.status(world.fighter))
    {
    case ManeuverStatus::Running: status = "Running"; break;
    case ManeuverStatus::Satisfied: status = "Satisfied"; break;
    case ManeuverStatus::Done: status = "Done"; break;
    }
    std::cout << std::fixed << std::setprecision(1) << "[maneuver-test] t=" << world.sim_time_s
              << " step=" << pilot.leg_index() + 1 << " " << pilot.leg_name()
              << " " << pilot.elapsed_s() << "/" << pilot.duration_s() << " s"
              << " " << status
              << " hdg=" << world.fighter.kinematics.heading_deg
              << " cmd=" << world.fighter.command.heading_deg()
              << " ATA=" << world.fighter.picture.ata_deg
              << " TA=" << world.fighter.picture.ta_deg
              << " LOS=" << world.fighter.picture.los_rate_deg_s;
    if (pilot.shot_failed()) { std::cout << " NO_TEST_MISSILE"; }
    std::cout << std::endl;
}
