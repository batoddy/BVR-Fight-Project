#include "io/console_log.hpp"

#include <iomanip>
#include <iostream>

namespace
{
    // Tek satırda uçağın durumu.
    void print_aircraft(const Aircraft& aircraft)
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

    // Havadaki füzelerim, atış sırasıyla: sıra, slot, hedefe uzaklık, pitbull.
    void print_active_missiles(const Aircraft& own)
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
