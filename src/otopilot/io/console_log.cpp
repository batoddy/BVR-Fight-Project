#include "io/console_log.hpp"
#include <iomanip>
#include <iostream>

// Tek satırda uçağın durumu. Yalnız bu dosyada kullanılır (static).
static void print_aircraft(const Aircraft& a)
{
    const Kinematics& k = a.kinematics;
    std::cout << a.id
              << "  lat " << std::setprecision(4) << k.lat_deg
              << "  lon " << k.lon_deg
              << "  alt " << std::setprecision(0) << k.altitude_ft << " ft"
              << "  hdg " << std::setprecision(1) << k.heading_deg_180
              << "  tas " << std::setprecision(0) << k.tas_kt << " kt"
              << "  rng " << std::setprecision(1) << a.enemy.range_3d_nm << " NM"
              << "  brg " << a.enemy.bearing_deg
              << "  kalan " << a.missiles_left() << '/' << a.missiles.size();
}

// Son atılan füze: slot, tip, Python id'si, kinematik geldi mi, hedefe uzaklık, sonuç. Yalnız bu dosyada.
static void print_last_missile(const Aircraft& own)
{
    const Missile* m = own.last_fired();
    if (m == nullptr)
    {
        return;
    }

    std::cout << "  | fuze slot " << m->slot << ' ' << m->profile.name;
    if (!m->has_kinematics)
    {
        std::cout << " (kinematik yok)";
        return;
    }

    std::cout << ' ' << m->python_id
              << (m->in_flight() ? " havada" : " bitti")
              << "  hedefe " << std::setprecision(1)
              << m->target.range_nm << " NM";
    if (m->hit())
    {
        std::cout << "  VURDU";
    }
    else if (m->lost())
    {
        std::cout << "  kaybetti";
    }
}

void ConsoleLog::print(const World& world)
{
    int second = static_cast<int>(world.sim_time_s);
    if (second == last_print_s_)
    {
        return;
    }

    last_print_s_ = second;
    std::cout << std::fixed << "t " << std::setw(3) << second << " s | ";
    print_aircraft(world.fighter);
    std::cout << " | ";
    print_aircraft(world.hostile);
    print_last_missile(world.fighter);
    std::cout << '\n';
}
