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
    /// İki uçağın command'ı, weapons.has_launch_order'ı (hedef karşı uçak) ve hesaplar (CalculationWriter).
    static Message write(std::uint64_t tick, const World& world);

private:
    /// Python komutu: {"id", "heading_deg", "altitude_m", "throttle", ["g_limit"]}; ft → m burada yapılır.
    static Json to_json(const std::string& id, const FlightCommand& command);
};
