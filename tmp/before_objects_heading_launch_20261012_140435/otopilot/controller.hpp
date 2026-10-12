#pragma once

#include "messages.hpp"

class Controller {
public:
    Message calculate(const Message& state);

private:
    Json targets_ = Json::array();  // Bir sonraki döngüde de korunacak hedefler.
};
