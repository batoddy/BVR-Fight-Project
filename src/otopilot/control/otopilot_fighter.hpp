#pragma once

#include "model/aircraft.hpp"
#include "control/maneuver.hpp"
#include "model/orders.hpp"
#include "model/world.hpp"

// Yalnız fighter'ın kararlarını ve uçuş komutunu yönetir.
class OtopilotFighter
{
public:
    FlightCommand step(
        Aircraft& fighter,
        const World& world,
        const FlightCommand& current);

private:
    FlightCommand command_;
    Maneuvers maneuvers_;
    int state_ = 0;
};
