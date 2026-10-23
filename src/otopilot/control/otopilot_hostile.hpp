#pragma once

#include "model/aircraft.hpp"
#include "control/maneuver.hpp"
#include "model/orders.hpp"
#include "model/world.hpp"

// Yalnız hostile'in kararlarını ve uçuş komutunu yönetir.
class OtopilotHostile
{
public:
    FlightCommand step(
        Aircraft& hostile,
        const World& world,
        const FlightCommand& current);

private:
    FlightCommand command_;
    Maneuvers maneuvers_;
    int state_ = 0;
};
