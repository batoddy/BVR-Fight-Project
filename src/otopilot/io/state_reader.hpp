#pragma once

#include <optional>
#include <string>

#include "io/messages.hpp"
#include "model/flight_command.hpp"
#include "model/world.hpp"

// JSON -> C++ nesneleri. Nesne oluşturmaz: verilen nesnenin içini doldurur.
// Protokol v2: docs/mimari/MESAJ_PROTOKOLU_V2.md. read_hello bir kez, read_world her step.
class StateReader
{
public:
    static void read_hello(const Json& data, World& world);   // protokol sürümü; iki uçağın kimliği, profili ve füze slotları
    static void read_world(const Json& data, World& world);   // saat ve her uçağın kendi paketi (own, weapons, warnings, events)

private:
    static void read_hello_aircraft(const Json& json, Aircraft& aircraft);   // kimlik, profil; slotlar weapons[].designation'dan
    static void read_packet(const Json& packet, Aircraft& aircraft);         // bir uçağın paketi
    static FlightCommand read_flight_command(const Json& json);             // own.applied_command; g_limit okunmaz, mode "direct" ise bank_deg ve g
    static void read_kinematics(const Json& json, Kinematics& kinematics);  // KIN: konum ve hareket alanları

    // Slot bazında sim'in füzeleri: durum, kinematik, kilit, pitbull. Hazırdan atılmışa geçen slot Weapons::record_launch'a gider.
    static void read_weapons(const Json& weapons, Aircraft& aircraft);

    static MissileState read_missile_state(const std::string& text);         // "ready" / "in_flight" / "ended"; bilinmeyende hata
    static GuidanceMode read_guidance_mode(const std::string& text);         // "none" / "datalink" / "inertial" / "seeker"; bilinmeyende hata
    static std::optional<MissileOutcome> read_outcome(const Json& json);     // null → yok, "hit" / "lost"; bilinmeyende hata
};
