#include "io/command_writer.hpp"
#include "io/calculation_writer.hpp"
#include <cmath>
#include <stdexcept>
#include "common/util_functions.hpp"
#include "common/roles.hpp"

Message CommandWriter::write(std::uint64_t tick, const World& world, const FlightCommand& fighter, const FlightCommand& hostile)
{
    Json data;
    data["aircraft"] = Json::array({to_json(FIGHTER_ID, fighter), to_json(HOSTILE_ID, hostile)});

    // 1v1: hedef hep karşı uçak. Python hazır füze yoksa atmaz.
    data["launch"] = Json::array();
    if (world.fighter.launch_order)
    {
        data["launch"].push_back({{"id", FIGHTER_ID}, {"target_id", HOSTILE_ID}});
    }
    if (world.hostile.launch_order)
    {
        data["launch"].push_back({{"id", HOSTILE_ID}, {"target_id", FIGHTER_ID}});
    }

    // Python uygulamaz; kayda (calculations.csv) yazar.
    data["calculations"] = CalculationWriter::write(world);

    Message message;
    message.kind = "command";
    message.tick = tick;              // Python aynı tick'i bekler
    message.data = data;
    return message;
}

Json CommandWriter::to_json(const std::string& id, const FlightCommand& command)
{
    // Python geçersiz komutta durur ama sebebini söyleyemez; burada uçak adıyla söyle.
    bool finite = std::isfinite(command.heading_deg_180) && std::isfinite(command.altitude_ft)
                  && std::isfinite(command.throttle);
    if (!finite)
    {
        throw std::runtime_error(id + ": komut sayi degil (NaN/sonsuz)");
    }

    if (command.throttle < 0.0 || command.throttle > 1.0)
    {
        throw std::runtime_error(id + ": throttle 0..1 disinda: " + std::to_string(command.throttle));
    }

    Json j;
    j["id"] = id;
    j["heading_deg"] = wrap_360(command.heading_deg_180);   // ±180 -> 0..360
    j["altitude_m"] = feet_to_meter(command.altitude_ft); // Python komutu metre bekler.
    j["throttle"] = command.throttle;                   // 0..1
    
    // İsteğe bağlı: 0 ise alan yazılmaz, Python varsayılanı kullanır.
    if (command.g_limit > 0.0)
    {
        if (command.g_limit < 1.5 || command.g_limit > 9.0)
        {
            throw std::runtime_error(id + ": g_limit 1.5..9 disinda: " + std::to_string(command.g_limit));
        }
        j["g_limit"] = command.g_limit;
    }

    return j;
}
