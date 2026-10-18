#pragma once

#include <string>
#include "messages.hpp"
#include "orders.hpp"
#include "world.hpp"

// JSON -> C++ nesneleri. Nesne oluşturmaz: verilen nesnenin içini doldurur.
// Her step çağrılır. Hiçbir şey saklamadığı için fonksiyonlar static.
class StateReader
{
public:
    // JSON'dan bütün dünyayı oku, world'ün içine yaz: saat, fighter, bogey.
    static void read_world(const Json& data, World& world);

    // data.flight_commands içinden id'si verilen uçağın son uygulanan komutunu oku, command'a yaz.
    static void read_flight_command(const Json& data, const std::string& id, FlightCommand& command);

private:
    // data.aircraft[i] -> aircraft: kimlik ve kinematik
    static void read_aircraft(const Json& j, Aircraft& aircraft);

    // Konum ve hareket alanları -> k (uçakta en üst seviyede durur)
    static void read_kinematics(const Json& j, Kinematics& k);

    // data.missiles içinden sahibi owner_id olanlar -> out (liste her step baştan yazılır)
    static void read_missiles(const Json& missiles, const std::string& owner_id, std::vector<Missile>& out);

    // Tek füze -> missile
    static void read_missile(const Json& j, Missile& missile);

};
