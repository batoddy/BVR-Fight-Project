#include "control/sim.hpp"

#include "io/command_writer.hpp"
#include "io/state_reader.hpp"

Message Sim::step(const Message& state)
{
    // 1. Ölçümler ve Python'un son uyguladığı uçuş komutları.
    StateReader::read_world(state.data, world_);

    // 2. Düşman ataması, füze bilgileri ve iki uçağın picture'ı.
    world_.update();
    // log_.print(world_);

    // 3. Her otopilot kendi uçağının komutunu, fazını ve atış emrini yazar.
    otopilot_fighter_.step(world_.fighter, world_.hostile);
    otopilot_hostile_.step(world_.hostile, world_.fighter);

    // 4. Uçuş komutları, atış emirleri ve hesaplar.
    return CommandWriter::write(state.tick, world_);
}
