#pragma once

#include "io/messages.hpp"
#include "model/world.hpp"

// C++ hesapları -> command mesajının "calculations" alanı.
// Python bunları uygulamaz; yalnız kayda (calculations.csv) yazar, görüntüleyici gösterir.
// Zaman damgasını Python koyar: hesap, gönderdiği state'in zamanına aittir.
class CalculationWriter
{
public:
    static Json write(const World& world);                             // {"aircraft": [fighter'ın bakışı, hostile'ın bakışı]}

private:
    static Json to_json(const Aircraft& own, const Aircraft& enemy);   // bir uçağın karşı uçağa göre hesapları ve çizim sabitleri
};
