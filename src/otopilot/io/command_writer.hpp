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
    static Json to_json(const std::string& id, const FlightCommand& command);  // {"id", "heading_deg", "altitude_m", "throttle", ["g_limit"]}; ft → m
};
