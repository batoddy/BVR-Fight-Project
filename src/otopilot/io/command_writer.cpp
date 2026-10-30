#include "io/command_writer.hpp"

#include <cmath>
#include <stdexcept>

#include "common/angles.hpp"
#include "common/roles.hpp"
#include "common/units.hpp"
#include "io/calculation_writer.hpp"

Message CommandWriter::write(std::uint64_t tick, const World& world)
{
    Json data;
    data["aircraft"] = Json::array({to_json(FIGHTER_ID, world.fighter.command),
                                    to_json(HOSTILE_ID, world.hostile.command)});

    // 1v1: hedef hep karşı uçak. Python hazır füze yoksa atmaz.
    data["launch"] = Json::array();
    if (world.fighter.weapons.has_launch_order)
    {
        data["launch"].push_back({{"id", FIGHTER_ID}, {"target_id", HOSTILE_ID}});
    }
    if (world.hostile.weapons.has_launch_order)
    {
        data["launch"].push_back({{"id", HOSTILE_ID}, {"target_id", FIGHTER_ID}});
    }

    // Python uygulamaz; kayda (calculations.csv) yazar.
    data["calculations"] = CalculationWriter::write(world);

    Message message;
    message.kind = "command";
    message.tick = tick;   // Python aynı tick'i bekler
    message.data = data;
    return message;
}

Json CommandWriter::to_json(const std::string& id, const FlightCommand& command)
{
    // Aralıkları FlightCommand'ın setter'ları korur; NaN/sonsuz ise setter'dan da geçer, burada yakala.
    const bool finite = std::isfinite(command.heading_deg()) && std::isfinite(command.altitude_ft())
                        && std::isfinite(command.throttle());
    if (!finite)
    {
        throw std::runtime_error(id + ": komut sayi degil (NaN/sonsuz)");
    }

    Json json;
    json["id"] = id;
    json["heading_deg"] = wrap_360(command.heading_deg());          // ±180 -> 0..360
    json["altitude_m"] = feet_to_meter(command.altitude_ft());      // Python komutu metre bekler
    json["throttle"] = command.throttle();                          // 0..1

    // İsteğe bağlı: yoksa alan yazılmaz, Python varsayılanı kullanır.
    if (const std::optional<double> g_limit = command.g_limit())
    {
        json["g_limit"] = *g_limit;
    }

    return json;
}
