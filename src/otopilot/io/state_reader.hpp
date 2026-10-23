#pragma once

#include <string>
#include "io/messages.hpp"
#include "model/orders.hpp"
#include "model/world.hpp"

// JSON -> C++ nesneleri. Nesne oluşturmaz: verilen nesnenin içini doldurur.
// Her step çağrılır. Hiçbir şey saklamadığı için fonksiyonlar static.
class StateReader
{
public:
    // JSON'dan bütün dünyayı oku, world'ün içine yaz: saat, fighter, hostile.
    static void read_world(const Json& data, World& world);

    // data.flight_commands içinden id'si verilen uçağın son uygulanan komutunu oku, command'a yaz.
    static void read_flight_command(const Json& data, const std::string& id, FlightCommand& command);

private:
    // data.aircraft[i] -> aircraft: kimlik ve kinematik
    static void read_aircraft(const Json& j, Aircraft& aircraft);

    // Konum ve hareket alanları -> k (uçakta en üst seviyede durur)
    static void read_kinematics(const Json& j, Kinematics& k);

    // data.missiles içinde sahibi aircraft olan füzeler -> aircraft.missiles, launch_order.
    // İlk step slotları kurar (sayı Python'dan, tipler profile.loadout'tan). Sonra UÇAN füze bir
    // slota eşleşir: Python id'si daha önce eşleşmişse o slot güncellenir; yeni beliren, atılmış
    // ama henüz eşleşmemiş en eski atışa bağlanır. Hazır (ready) füzeler okunmaz.
    static void read_missiles(const Json& data, Aircraft& aircraft);

    // "ready" / "in_flight" / "ended" -> MissileState; bilinmeyen metinde hata
    static MissileState read_missile_state(const std::string& text);

};
