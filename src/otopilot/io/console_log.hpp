#pragma once

#include <cstddef>

#include "model/world.hpp"

class OtopilotManeuverTest;

// Terminale saniyede bir satır: iki uçağın durumu ve fighter'ın havadaki füzeleri.
// Her step yazarsak terminal saniyede ~12 satırla dolar; bu yüzden son yazılan saniyeyi tutar.
class ConsoleLog
{
public:
    void print(const World& world);   // yeni bir simülasyon saniyesine geçildiyse bir satır yaz
    void print_maneuver_test(const World& world, const OtopilotManeuverTest& pilot); // saniyede bir ve her adım geçişinde test sonucu

private:
    int last_print_s_ = -1;   // terminale en son yazılan simülasyon saniyesi
    int last_test_print_s_ = -1;
    std::size_t last_test_leg_ = 0;
};
