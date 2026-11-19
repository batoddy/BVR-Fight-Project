#include "io/command_writer.hpp"

#include <cmath>
#include <stdexcept>

#include "common/angles.hpp"
#include "io/roles.hpp"
#include "common/units.hpp"
#include "io/calculation_writer.hpp"

Message CommandWriter::write(std::uint64_t tick, const World& world)
{
    Json data;
    data["aircraft"] = Json::array({to_json(FIGHTER_ID, world.fighter.command),
                                    to_json(HOSTILE_ID, world.hostile.command)});

    data["launch"] = Json::array();   // 1v1: hedef hep karşı uçak; sim ilk hazır füzeyi atar
    add_launches(FIGHTER_ID, HOSTILE_ID, world.fighter.weapons, data["launch"]);
    add_launches(HOSTILE_ID, FIGHTER_ID, world.hostile.weapons, data["launch"]);

    data["calculations"] = CalculationWriter::write(world);   // Python uygulamaz; kayda (calculations.csv) yazar

    Message message;
    message.kind = "command";
    message.tick = tick;   // Python aynı tick'i bekler
    message.data = data;
    return message;
}

void CommandWriter::add_launches(const std::string& id, const std::string& target_id, const Weapons& weapons, Json& launch)
{
    if (weapons.has_launch_order)
    {
        launch.push_back({{"id", id}, {"target_id", target_id}});
    }
}

Json CommandWriter::to_json(const std::string& id, const FlightCommand& command)
{
    if (command.mode() == CommandMode::Direct)
    {
        return to_direct_json(id, command);
    }

    const bool finite = std::isfinite(command.heading_deg()) && std::isfinite(command.altitude_ft())
                        && std::isfinite(command.throttle());   // aralığı setter'lar korur; NaN/sonsuz onlardan da geçer
    if (!finite)
    {
        throw std::runtime_error(id + ": komut sayi degil (NaN/sonsuz)");
    }

    Json json;
    json["id"] = id;
    json["mode"] = "target";
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

Json CommandWriter::to_direct_json(const std::string& id, const FlightCommand& command)
{
    const double bank_deg = command.bank_deg();
    const double g = command.g();
    const bool valid = std::isfinite(command.throttle())
                       && std::isfinite(bank_deg) && bank_deg >= -180.0 && bank_deg <= 180.0
                       && std::isfinite(g) && g >= DIRECT_G_MIN && g <= DIRECT_G_MAX;   // NaN setter'lardan geçer
    if (!valid)
    {
        throw std::runtime_error(id + ": dogrudan komut gecersiz (bank -180..180, g -4..9)");
    }

    Json json;
    json["id"] = id;
    json["mode"] = "direct";
    json["bank_deg"] = bank_deg;              // derece, + sağ
    json["g"] = g;                            // g
    json["throttle"] = command.throttle();    // 0..1
    return json;
}
