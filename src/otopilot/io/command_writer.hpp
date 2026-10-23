#pragma once

#include <cstdint>
#include <string>
#include "io/messages.hpp"
#include "model/orders.hpp"
#include "model/world.hpp"

// C++ emirleri -> Python'un beklediği command mesajı.
class CommandWriter
{
public:
    // Uçuş komutları, uçakların launch_order'ı (hedef karşı uçak) ve hesaplar (CalculationWriter).
    static Message write(std::uint64_t tick, const World& world, const FlightCommand& fighter, const FlightCommand& hostile);


private:
    // Python komutu: {"id", "heading_deg", "altitude_m", "throttle"}; ft → m burada yapılır.
    static Json to_json(const std::string& id, const FlightCommand& command);
};
