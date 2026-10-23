#include "control/sim.hpp"
#include "io/command_writer.hpp"
#include "model/orders.hpp"
#include "common/roles.hpp"
#include "io/state_reader.hpp"

Message Sim::step(const Message& state)
{
    // 1. İki uçağın ölçümlerini oku.
    StateReader::read_world(state.data, world_);

    // 2. Python'un son uyguladığı uçuş komutlarını oku.
    FlightCommand fighter_current;
    FlightCommand hostile_current;

    StateReader::read_flight_command(
        state.data, FIGHTER_ID, fighter_current);

    StateReader::read_flight_command(
        state.data, HOSTILE_ID, hostile_current);

    // 3. İki uçak için geometri ve silah hesaplarını güncelle.
    world_.update();
    // log_.print(world_);

    // 4. Her otopilot kendi uçağının kararını ve komutunu üretir.
    const FlightCommand fighter_command = otopilot_fighter_.step(
        world_.fighter, world_, fighter_current);

    const FlightCommand hostile_command = otopilot_hostile_.step(
        world_.hostile, world_, hostile_current);

    // 5. Uçuş komutlarını, atış emirlerini ve hesapları gönder.
    return CommandWriter::write(
        state.tick, world_, fighter_command, hostile_command);
}
