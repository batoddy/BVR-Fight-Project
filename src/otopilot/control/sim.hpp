#pragma once

#include "control/otopilot_fighter.hpp"
#include "control/otopilot_hostile.hpp"
#include "control/otopilot_maneuver_test.hpp"
#include "io/console_log.hpp"
#include "io/messages.hpp"
#include "model/world.hpp"

// Bir step: ölçümü okur, ortak hesapları günceller, iki otopilotu çalıştırır ve komut mesajını oluşturur.
class Sim
{
public:
    explicit Sim(bool maneuver_test = false) : maneuver_test_enabled_(maneuver_test) {}
    Message step(const Message& state);   // state mesajını işler, Python'a gidecek command mesajını döner

private:
    static constexpr bool PRINT_LOG = false;   // true: terminale saniyede bir durum satırı (ConsoleLog)

    World world_;
    bool maneuver_test_enabled_ = false;
    OtopilotManeuverTest maneuver_test_;

    OtopilotFighter otopilot_fighter_;
    OtopilotHostile otopilot_hostile_;

    ConsoleLog log_;
};
