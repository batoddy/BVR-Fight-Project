#pragma once

#include <string>

#include "model/world.hpp"

// Terminale saniyede bir satır: iki uçağın durumu ve fighter'ın havadaki füzeleri. Karar değişince ayrıca hemen bir satır.
// Her step yazarsak terminal saniyede ~12 satırla dolar; bu yüzden son yazılan saniyeyi tutar.
class ConsoleLog
{
public:
    void print(const World& world);   // yeni bir simülasyon saniyesine geçildiyse bir satır yaz

private:
    static void print_decision(const Aircraft& aircraft, std::string& last_decision, double sim_time_s); // karar değiştiyse hemen bir satır

    std::string last_fighter_decision_;   // terminale en son yazılan karar
    std::string last_hostile_decision_;
    int last_print_s_ = -1;   // terminale en son yazılan simülasyon saniyesi
};
