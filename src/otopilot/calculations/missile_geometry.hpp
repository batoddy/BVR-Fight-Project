#pragma once

#include <optional>

#include "model/aircraft.hpp"
#include "model/missile.hpp"

// Atılmış füze ile hedefi arasındaki hesaplar. Saf fonksiyonlar.
//   missile: atılmış füze (has_kinematics true olmalı)
//   target:  füzenin hedefi olan uçak
// Mesafe ve kapanma geometry'nin aynı fonksiyonlarıyla, füzenin Kinematics'i üzerinden bulunur.
namespace missile_geometry
{
    // NM, füze ile hedef arası 3B mesafe
    double range_nm(const Missile& missile, const Aircraft& target);

    // kt, füze ile hedef arası mesafenin azalma hızı; + yaklaşıyor
    double closure_kt(const Missile& missile, const Aircraft& target);

    // s, bu kapanma hızı sürerse hedefe kalan süre: range / closure. Kapanma yoksa yok.
    std::optional<double> time_to_go_s(double range_nm, double closure_kt);

    // Füze pitbull mı: hedefe pitbull_range_nm'den yakın. pitbull_range_nm 0 ise (Fox1, Fox2 tipleri) hep false.
    bool is_pitbull(const Missile& missile, double range_nm);
}
