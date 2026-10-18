#include "autopilot.hpp"
#include <iomanip>
#include <iostream>
#include "command_writer.hpp"
#include "roles.hpp"
#include "state_reader.hpp"

// Tek satırda uçağın durumu. Yalnız bu dosyada kullanılır (static).
static void print_aircraft(const Aircraft& a)
{
    const Kinematics& k = a.kinematics;
    std::cout << a.id
              << "  lat " << std::setprecision(4) << k.lat_deg
              << "  lon " << k.lon_deg
              << "  alt " << std::setprecision(0) << k.altitude_m << " m"
              << "  hdg " << std::setprecision(1) << k.heading_deg_180
              << "  tas " << std::setprecision(0) << k.tas_mps << " m/s"
              << "  rng " << std::setprecision(1) << a.enemy.range_3d_m / 1000.0 << " km"
              << "  brg " << a.enemy.bearing_deg
              << "  msl " << a.own.ready_missiles << " hazir " << a.own.missile_in_flight << " havada";

}

// otopilot tarafından verilerin okunduğu hesaplanıp yeni komutun üretildiği adım fonksiyonu.
Message Autopilot::step(const Message& state)
{
    // 1. Ölçüm.
    StateReader::read_world(state.data, world_);

    // Python her step son uygulanan komutları geri gönderir:
    //   fighter: bir önceki step bizim verdiğimiz komut (ilk step'te YAML başlangıcı)
    //   bogey:   hiç değiştirmediğimiz için hep YAML başlangıcı (hold)
    // Manevra current olarak bunu alır; bu yüzden manevradan önce okunmalı.
    StateReader::read_flight_command(state.data, FIGHTER_ID, fighter_command_);
    StateReader::read_flight_command(state.data, BOGEY_ID, bogey_command_);

    // 2. Hesap: ölçümden türetilen bilgiler (enemy).
    world_.update();

    // Saniyede bir yaz; her step yazarsak terminal saniyede ~12 satırla dolar.
    int second = static_cast<int>(world_.sim_time_s);
    if (second != last_print_s_)
    {
        last_print_s_ = second;
        std::cout << std::fixed << "t " << std::setw(3) << second << " s | ";
        print_aircraft(world_.fighter);
        std::cout << " | ";
        print_aircraft(world_.bogey);
        std::cout << '\n';
    }



    
    // 3. Manevra: elle seçilir. Denemek istediğini aç, diğerini kapat.
    if(world_.sim_time_s < 20)
    {
        fighter_command_ = maneuvers_.hold.compute(world_.fighter, fighter_command_);
    }

    else
    {
        fighter_command_ = maneuvers_.crank.compute(world_.fighter, fighter_command_);
    }

    // 4. Atış: elle. Tek seferlik; her step yeniden karar verilir.
    bool fighter_fire = false;
    // bool fighter_fire = world_.sim_time_s > 10.0 && world_.sim_time_s < 11.0;   // test: 10. saniyede bir atış





    // 5. Komut.
    return CommandWriter::write(state.tick, fighter_command_, bogey_command_, fighter_fire);
}
