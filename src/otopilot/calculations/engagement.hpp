#pragma once

#include "model/aircraft.hpp"

// Füze ve taktik hesapları: WEZ, MAR, DOR. Hiçbir şey saklamaz; fonksiyonlar static.
//
// Geometry'den farkı: yalnız konuma değil, uçakların profiline (füze menzili, radar) de bakar.
// Bu yüzden Kinematics değil, Aircraft alır.
//   own:   bakan taraf
//   enemy: karşı taraf
// own.enemy.range_3d_nm gibi Geometry sonuçları burada çağrılmadan önce doludur (World::update sırası).
class Engagement
{
public:
    // own'un WEZ'i: ilk füzesine (missiles[0]) göre, bu düşmana karşı.
    //   min_nm: füzenin iç sınırı
    //   max_nm: düşman bugünkü rotasını sürdürürse azami menzil; düşman bana geliyorsa
    //          (own.enemy.rate_of_closure_kt +) uzar, kaçıyorsa kısalır
    // Füzesi yoksa {0, 0}: hiçbir menzil WEZ'de değil.
    static Wez wez(const Aircraft& own);

    // Bu menzil WEZ'in içinde mi: min_nm < range_nm < max_nm
    static bool in_wez(double range_nm, const Wez& wez);

    static double f_pole_nm(const Aircraft& own, const Aircraft& enemy);


    // NM, Minimum Abort Range
    static double mar_nm(const Aircraft& own, const Aircraft& enemy);

    // NM, Desired Out Range
    static double dor_nm(const Aircraft& own, const Aircraft& enemy);
};
