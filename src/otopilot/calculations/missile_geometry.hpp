#pragma once

#include "model/aircraft.hpp"

// Atılmış füze ile hedefi arasındaki hesaplar. Hiçbir şey saklamaz; fonksiyonlar static.
//
// Geometry'den farkı: füzenin profiline de (pitbull menzili) bakar. Bu yüzden Missile alır.
//   missile: atılmış füze (has_kinematics true olmalı)
//   target:  füzenin hedefi olan uçak
// Mesafe ve kapanma Geometry'nin aynı fonksiyonlarıyla, füzenin Kinematics'i üzerinden bulunur.
class MissileGeometry
{
public:
    // NM, füze ile hedef arası 3B mesafe
    static double range_nm(const Missile& missile, const Aircraft& target);

    // kt, füze ile hedef arası mesafenin azalma hızı; + yaklaşıyor
    static double closure_kt(const Missile& missile, const Aircraft& target);

    // s, bu kapanma hızı sürerse hedefe kalan süre: range / closure. Kapanma yoksa -1.
    static double time_to_go_s(double range_nm, double closure_kt);

    // Füze pitbull mı: hedefe pitbull_range_nm'den yakın. pitbull_range_nm 0 ise (FOX1, FOX2) hep false.
    static bool pitbull(const Missile& missile, double range_nm);

    // Füze los m: hedefin füzeye göre los (line-of-sight) vektörü
    static Ned los_nm(const Missile& missile, const Aircraft& target);
};
