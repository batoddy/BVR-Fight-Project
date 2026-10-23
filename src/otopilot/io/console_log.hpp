#pragma once

#include "model/world.hpp"

// Terminale saniyede bir satır: iki uçağın durumu ve fighter'ın son füzesi.
// Her step yazarsak terminal saniyede ~12 satırla dolar; bu yüzden son yazılan saniyeyi tutar.
class ConsoleLog
{
public:
    // Yeni bir simülasyon saniyesine geçildiyse bir satır yaz; değilse bir şey yapmaz.
    void print(const World& world);

private:
    int last_print_s_ = -1;   // terminale en son yazılan simülasyon saniyesi
};
