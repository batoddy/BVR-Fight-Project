#pragma once

#include <cstdint>
#include <string>

#include "io/messages.hpp"
#include "model/flight_command.hpp"
#include "model/world.hpp"

// C++ emirleri -> Python'un beklediği command mesajı.
class CommandWriter
{
public:
    static Message write(std::uint64_t tick, const World& world);              // iki uçağın komutu, atış emirleri, hesaplar

private:
    static void add_launches(const std::string& id, const std::string& target_id, const Weapons& weapons, Json& launch);   // {"id", "target_id"}
    static Json to_json(const std::string& id, const FlightCommand& command);         // {"id", "mode": "target", "heading_deg", "altitude_m", "throttle", ["g_limit"]}; ft → m
    static Json to_direct_json(const std::string& id, const FlightCommand& command);  // {"id", "mode": "direct", "bank_deg", "g", "throttle"}
};
