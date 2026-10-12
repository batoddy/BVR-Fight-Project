#pragma once

#include "messages.hpp"
#include "objects.hpp"

class Controller {
public:
    Message calculate(const Message& state);

private:
    // Bir sonraki döngüde de korunacak uçuş hedefleri. heading ±180.
    FlightCommand fighter_command_;   // blue
    FlightCommand bogey_command_;     // red
};
