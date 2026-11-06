#pragma once

#include "model/world.hpp"

// Terminale saniyede bir satır: iki uçağın durumu ve fighter'ın havadaki füzeleri.
// Her step yazarsak terminal saniyede ~12 satırla dolar; bu yüzden son yazılan saniyeyi tutar.
class ConsoleLog
{
public:
    void print(const World& world);   // yeni bir simülasyon saniyesine geçildiyse bir satır yaz

private:
    int last_print_s_ = -1;   // terminale en son yazılan simülasyon saniyesi
};
