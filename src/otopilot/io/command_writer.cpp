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

    data["launch"] = Json::array();   // 1v1: hedef hep karşı uçak; Python hazır füze yoksa atmaz
    if (world.fighter.weapons.has_launch_order)
    {
        data["launch"].push_back({{"id", FIGHTER_ID}, {"target_id", HOSTILE_ID}});
    }
    if (world.hostile.weapons.has_launch_order)
    {
        data["launch"].push_back({{"id", HOSTILE_ID}, {"target_id", FIGHTER_ID}});
    }

    data["calculations"] = CalculationWriter::write(world);   // Python uygulamaz; kayda (calculations.csv) yazar

    Message message;
    message.kind = "command";
    message.tick = tick;   // Python aynı tick'i bekler
    message.data = data;
    return message;
}

Json CommandWriter::to_json(const std::string& id, const FlightCommand& command)
{
    const bool finite = std::isfinite(command.heading_deg()) && std::isfinite(command.altitude_ft())
                        && std::isfinite(command.throttle());   // aralığı setter'lar korur; NaN/sonsuz onlardan da geçer
    if (!finite)
    {
        throw std::runtime_error(id + ": komut sayi degil (NaN/sonsuz)");
    }

    Json json;
    json["id"] = id;
    json["heading_deg"] = wrap_360(command.heading_deg());          // ±180 -> 0..360
    json["altitude_m"] = feet_to_meter(command.altitude_ft());      // Python komutu metre bekler
    json["throttle"] = command.throttle();                          // 0..1

    if (const std::optional<double> g_limit = command.g_limit())   // yoksa alan yazılmaz, Python varsayılanı
    {
        if (!std::isfinite(*g_limit))
        {
            throw std::runtime_error(id + ": g_limit sayi degil (NaN/sonsuz)");
        }
        json["g_limit"] = *g_limit;
    }

    return json;
}
