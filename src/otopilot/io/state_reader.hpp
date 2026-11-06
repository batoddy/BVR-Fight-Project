#pragma once

#include <string>

#include "io/messages.hpp"
#include "model/flight_command.hpp"
#include "model/world.hpp"

// JSON -> C++ nesneleri. Nesne oluşturmaz: verilen nesnenin içini doldurur. Her step çağrılır.
class StateReader
{
public:
    static void read_world(const Json& data, World& world);   // saat, iki uçak, füzeler ve her uçağın Python'un son uyguladığı komutu

private:
    static FlightCommand read_flight_command(const Json& data, const std::string& id);   // data.flight_commands'tan o uçağın komutu
    static void read_aircraft(const Json& json, Aircraft& aircraft);                      // kimlik ve kinematik; profil yalnız tip değişince
    static void read_kinematics(const Json& json, Kinematics& kinematics);                // konum ve hareket alanları

    // data.missiles içinde sahibi aircraft olan füzeler -> aircraft.weapons.missiles. İlk step slotları kurar
    // (sayı Python'dan, tipler profile.loadout'tan). Sonra uçan füze bir slota eşleşir: Python id'si daha önce
    // eşleşmişse o slot güncellenir; yeni beliren, atılmış ama eşleşmemiş en eski atışa bağlanır. Hazır füzeler okunmaz.
    static void read_missiles(const Json& data, Aircraft& aircraft);

    static MissileState read_missile_state(const std::string& text);       // "ready" / "in_flight" / "ended"; bilinmeyende hata
    static MissileOutcome read_missile_outcome(const std::string& text);   // "hit" / "lost"; bilinmeyende hata (null önceden elenir)
};
