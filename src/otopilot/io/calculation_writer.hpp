#pragma once

#include "io/messages.hpp"
#include "model/world.hpp"

// C++ hesapları -> command mesajının "calculations" alanı.
// Python bunları uygulamaz; yalnız kayda (calculations.csv) yazar, görüntüleyici gösterir.
// Zaman damgasını Python koyar: hesap, gönderdiği state'in zamanına aittir.
class CalculationWriter
{
public:
    /// {"aircraft": [fighter'ın hostile'e bakışı, hostile'in fighter'a bakışı]}
    static Json write(const World& world);

private:
    /// Tek uçağın karşı uçağa göre hesapları ve çizim için profil sabitleri
    static Json to_json(const Aircraft& own, const Aircraft& enemy);
};
