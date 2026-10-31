#pragma once

#include <string>

#include "io/messages.hpp"
#include "model/flight_command.hpp"
#include "model/world.hpp"

// JSON -> C++ nesneleri. Nesne oluşturmaz: verilen nesnenin içini doldurur. Her step çağrılır.
class StateReader
{
public:
    /// JSON'dan bütün dünyayı oku, world'ün içine yaz: saat, fighter, hostile, füzeler ve
    /// her uçağın Python'un son uyguladığı komutu (aircraft.command).
    static void read_world(const Json& data, World& world);

private:
    /// data.flight_commands içinden id'si verilen uçağın son uygulanan komutunu oku.
    static FlightCommand read_flight_command(const Json& data, const std::string& id);

    /// data.aircraft[i] -> aircraft: kimlik ve kinematik; profil yalnız tip değişince
    static void read_aircraft(const Json& json, Aircraft& aircraft);

    /// Konum ve hareket alanları -> kinematics (uçakta en üst seviyede durur)
    static void read_kinematics(const Json& json, Kinematics& kinematics);

    /// data.missiles içinde sahibi aircraft olan füzeler -> aircraft.weapons.missiles; weapons.has_launch_order sıfırlanır.
    /// İlk step slotları kurar (sayı Python'dan, tipler profile.loadout'tan). Sonra UÇAN füze bir
    /// slota eşleşir: Python id'si daha önce eşleşmişse o slot güncellenir; yeni beliren, atılmış
    /// ama henüz eşleşmemiş en eski atışa bağlanır. Hazır (ready) füzeler okunmaz.
    static void read_missiles(const Json& data, Aircraft& aircraft);

    /// "ready" / "in_flight" / "ended" -> MissileState; bilinmeyen metinde hata
    static MissileState read_missile_state(const std::string& text);
};
