#include "command_writer.hpp"
#include <cmath>
#include <stdexcept>
#include "util_functions.hpp"
#include "roles.hpp"

Message CommandWriter::write(std::uint64_t tick, const FlightCommand& fighter, const FlightCommand& bogey, bool fighter_fire)
{
    Json data;
    data["aircraft"] = Json::array({to_json(FIGHTER_ID, fighter), to_json(BOGEY_ID, bogey)});
    
    data["launch"] = Json::array();
    if (fighter_fire)
    {
        // 1v1: hedef hep bogey. Python havada kendi füzesi varsa ya da hazır füze yoksa atmaz.
        data["launch"].push_back({{"id", FIGHTER_ID}, {"target_id", BOGEY_ID}});
    }


    Message message;
    message.kind = "command";
    message.tick = tick;              // Python aynı tick'i bekler
    message.data = data;
    return message;
}

Json CommandWriter::to_json(const std::string& id, const FlightCommand& command)
{
    // Python geçersiz komutta durur ama sebebini söyleyemez; burada uçak adıyla söyle.
    bool finite = std::isfinite(command.heading_deg_180) && std::isfinite(command.altitude_m)
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
    j["altitude_m"] = command.altitude_m;               // m
    j["throttle"] = command.throttle;                   // 0..1
    return j;
}
