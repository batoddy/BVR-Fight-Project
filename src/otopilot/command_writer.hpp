#pragma once

#include <cstdint>
#include <string>
#include "messages.hpp"
#include "orders.hpp"

// C++ emirleri -> Python'un beklediği command mesajı.
class CommandWriter
{
public:
    static Message write(std::uint64_t tick, const FlightCommand& fighter, const FlightCommand& bogey, bool fighter_fire);


private:
    // Tek uçağın komutu: {"id", "heading_deg", "altitude_m", "throttle"}
    static Json to_json(const std::string& id, const FlightCommand& command);
};
