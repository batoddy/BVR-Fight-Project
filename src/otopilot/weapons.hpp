#pragma once

#include "aircraft.hpp"

// Füze ve taktik hesapları: WEZ, MAR, DOR. Hiçbir şey saklamaz; fonksiyonlar static.
//
// Geometry'den farkı: yalnız konuma değil, uçakların profiline (füze menzili, radar) de bakar.
// Bu yüzden Kinematics değil, Aircraft alır.
//   own:   bakan taraf
//   enemy: karşı taraf
// own.enemy.range_m gibi Geometry sonuçları burada çağrılmadan önce doludur (World::update sırası).
class Weapons
{
public:
    // Karşı uçak benim füzemin etkin menzil aralığında mı
    static bool in_wez(const Aircraft& own, const Aircraft& enemy);

    // m, Minimum Abort Range
    static double mar_m(const Aircraft& own, const Aircraft& enemy);

    // m, Desired Out Range
    static double dor_m(const Aircraft& own, const Aircraft& enemy);
};
