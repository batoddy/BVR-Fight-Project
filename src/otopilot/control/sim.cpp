#include "control/sim.hpp"

#include "io/command_writer.hpp"
#include "io/state_reader.hpp"

Message Sim::step(const Message& state)
{
    StateReader::read_world(state.data, world_);             // 1. ölçümler ve Python'un son uyguladığı komutlar
    world_.update();                                          // 2. düşman ataması, füze bilgileri, picture, weapons
    if (PRINT_LOG)
    {
        log_.print(world_);
    }

    world_.fighter.weapons.has_launch_order = false;          // önceki step'in emri gitti; bu step'in emri yoksa gönderilmez
    world_.hostile.weapons.has_launch_order = false;
    otopilot_fighter_.step(world_.fighter);                  // 3. her otopilot komutunu, fazını ve atış emrini yazar
    otopilot_hostile_.step(world_.hostile);

    return CommandWriter::write(state.tick, world_);          // 4. uçuş komutları, atış emirleri ve hesaplar
}
