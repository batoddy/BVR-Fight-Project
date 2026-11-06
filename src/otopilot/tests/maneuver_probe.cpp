// Satır başına JSON: {state: Simulation.read_state(), mode: ..., angle_deg?: ..., fire_red?: true}.
// Yanıt, CommandWriter çıktısı ile mevcut manevra durumudur. Yalnız fizik doğrulamasında kullanılır.
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>

#include "calculations/geometry.hpp"
#include "control/maneuver.hpp"
#include "io/command_writer.hpp"
#include "io/state_reader.hpp"
#include "model/world.hpp"

namespace
{
    Maneuver& select(Maneuvers& maneuvers, const Json& request)
    {
        const std::string mode = request.at("mode").get<std::string>();
        if (mode == "hold") { return maneuvers.hold; }
        if (mode == "out") { return maneuvers.out; }
        if (mode == "recommit") { return maneuvers.recommit; }
        if (mode == "commit") { return maneuvers.commit; }
        if (mode == "crank") { return maneuvers.crank; }
        if (mode == "beam") { return maneuvers.beam; }
        if (mode == "notch") { return maneuvers.notch; }
        if (mode == "drag") { return maneuvers.drag; }
        if (mode == "collision") { return maneuvers.collision; }
        if (mode == "pure") { return maneuvers.pure_pursuit; }
        if (mode == "lag") { return maneuvers.lag_pursuit; }
        if (mode == "lead") { return maneuvers.lead_pursuit; }
        if (mode == "fly_heading")
        {
            maneuvers.fly_heading.set(request.at("angle_deg").get<double>());
            return maneuvers.fly_heading;
        }
        if (mode == "flank")
        {
            maneuvers.flank.set(request.at("angle_deg").get<double>());
            return maneuvers.flank;
        }
        if (mode == "offset")
        {
            const double signed_angle_deg = request.at("angle_deg").get<double>();
            ManeuverSide side = ManeuverSide::Right;
            if (signed_angle_deg < 0) { side = ManeuverSide::Left; }
            maneuvers.offset.set(std::abs(signed_angle_deg), side);
            return maneuvers.offset;
        }
        throw std::invalid_argument("Bilinmeyen deneme manevrasi: " + mode);
    }
}

int main()
{
    World world;
    Maneuvers own_maneuvers, enemy_maneuvers;
    std::string line;
    try
    {
        while (std::getline(std::cin, line))
        {
            const Json request = Json::parse(line);
            const Json& state = request.at("state");
            StateReader::read_world(state, world);
            world.fighter.weapons.has_launch_order = false;
            world.hostile.weapons.has_launch_order = false;
            if (request.value("fire_red", false))
            {
                if (!world.hostile.weapons.fire(world.hostile.profile.main_weapon))
                {
                    throw std::runtime_error("Deneme fuzesi atilamadi");
                }
            }
            world.update();
            Maneuver& maneuver = select(own_maneuvers, request);
            own_maneuvers.fly(world.fighter, maneuver);
            enemy_maneuvers.fly(world.hostile, enemy_maneuvers.hold);
            Json result = CommandWriter::write(state.at("tick").get<std::uint64_t>(), world).data;
            switch (maneuver.status(world.fighter))
            {
            case ManeuverStatus::Running: result["status"] = "running"; break;
            case ManeuverStatus::Satisfied: result["status"] = "satisfied"; break;
            case ManeuverStatus::Done: result["status"] = "done"; break;
            }
            result["metrics"] = {{"ata_deg", world.fighter.picture.ata_deg},
                {"ta_deg", world.fighter.picture.ta_deg}, {"los_rate_deg_s", world.fighter.picture.los_rate_deg_s},
                {"closure_kt", world.fighter.picture.closure_kt}, {"range_nm", world.fighter.picture.range_2d_nm},
                {"has_threat", world.fighter.picture.threat != nullptr}};
            std::cout << result.dump() << std::endl;
        }
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
