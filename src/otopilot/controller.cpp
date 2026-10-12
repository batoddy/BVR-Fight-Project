#include "controller.hpp"
#include "geometry.hpp"
#include <iostream>

Message Controller::calculate(const Message& state)
{
    // Gelen ölçümü nesnelere doldur. Yönler ±180: 0 kuzey, + sağ, - sol.
    World world = World::from_json(state.data);

    // İlk ölçümde Python/YAML başlangıç hedeflerini bir kez al.
    if (state.tick == 0)
    {
        for (const Json& j : state.data.at("flight_commands"))
        {
            FlightCommand command = FlightCommand::from_json(j);
            if (command.id == "blue")
            {
                fighter_command_ = command;
            }
            if (command.id == "red")
            {
                bogey_command_ = command;
            }
        }

        // Profil okumasını sadece ilk ölçümde yazdır; her adımda terminali doldurma.
        for (const Aircraft* aircraft : {&world.fighter, &world.bogey})
        {
            std::cout << aircraft->id << " | type: " << aircraft->type
                      << " | profil: " << aircraft->profile.name
                      << " | max Mach: " << aircraft->profile.max_mach
                      << " | max g: " << aircraft->profile.max_g << '\n';
        }
    }

    // SENİN HESAPLAMA KODUN BURAYA GELECEK.
    // Ölçüm:
    //   world.fighter.kinematics.alt_msl_m       world.fighter.kinematics.yaw_deg  (±180)
    //   world.bogey.kinematics.lat_deg           world.bogey.health
    //   world.fighter.profile.max_mach           (aircraft_profiles.hpp)
    //   for (const Missile& m : world.bogey_missiles)
    //   {
    //       if (m.kinematics)                    // yalnızca atılmış füzede var
    //       {
    //           m.kinematics->alt_msl_m;
    //       }
    //   }
    // Hedef (heading ±180: + sağ, - sol):
    //   fighter_command_.heading_deg = -90.0;   // batı
    //   fighter_command_.altitude_m = 9000.0;
    //   fighter_command_.throttle = 0.6;
    // Atış (tek seferlik; havada kendi füzesi varsa veya hazır füze yoksa Python uygulamaz):
    //   fighter_command_.fire(world.bogey);

    const Kinematics& own_k = world.fighter.kinematics;
    const Kinematics& target_k = world.bogey.kinematics;

    // double range = range_m(own_k, target_k);                   // 3B mesafe, m
    // double horizontal = horizontal_range_m(own_k, target_k);   // yatay mesafe, m
    // Ned los = line_of_sight(own_k, target_k);

    double bearing = bearing_deg(own_k,target_k);
  
    if (bearing < -world.fighter.profile.radar_gimbal_deg/2 + 2)
    {
        fighter_command_.heading_deg = bearing - world.fighter.profile.radar_gimbal_deg/2 + 3;
    }

    else if (bearing > -world.fighter.profile.radar_gimbal_deg/2 + 4)
    {
        fighter_command_.heading_deg = bearing - world.fighter.profile.radar_gimbal_deg/2 + 3;
    }

    // if (bearing > world.fighter.profile.radar_gimbal_deg/2 - 2)
    // {
    //     fighter_command_.heading_deg = world.fighter.profile.radar_gimbal_deg/2 -5;
    // }


    // Uçuş komutlarını Python'un beklediği JSON'a çevir. Heading burada 0..360'a döner.
    Json aircraft = Json::array();
    aircraft.push_back(fighter_command_.to_json());
    aircraft.push_back(bogey_command_.to_json());

    // Atış emirlerini topla ve temizle: tek seferlik, bir sonraki tura taşınmasın.
    Json launch = Json::array();
    for (FlightCommand* command : {&fighter_command_, &bogey_command_})
    {
        if (!command->fire_target_id.empty())
        {
            Json order;
            order["id"] = command->id;
            order["target_id"] = command->fire_target_id;
            launch.push_back(order);

            command->fire_target_id.clear();
        }
    }

    Json data;
    data["aircraft"] = aircraft;
    data["launch"] = launch;

    Message command_message;
    command_message.kind = "command";
    command_message.tick = state.tick;
    command_message.data = data;
    return command_message;
}
