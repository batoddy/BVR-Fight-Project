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
    /// NM, füze ile hedef arası 3B mesafe
    double range_nm(const Missile& missile, const Aircraft& target);

    /// kt, füze ile hedef arası mesafenin azalma hızı; + yaklaşıyor
    double closure_kt(const Missile& missile, const Aircraft& target);

    /// Füze pitbull mı: hedefe pitbull_range_nm'den yakın. pitbull_range_nm 0 ise (Fox1, Fox2 tipleri) hep false.
    bool is_pitbull(const Missile& missile, double range_nm);

    /// kt, füzenin boost bitişinden t_s saniye sonraki hızı
    double velocity_at_time(const MissileProfile& missile_profile, double t_s);

    /// NM, füzenin boost bitişinden t_s saniyede aldığı yol
    double distance_at_time(const MissileProfile& missile_profile, double t_s);

    /// s, füzenin boost bitişinden distance_nm yol alması için gereken süre; füze o yola erişemezse veya yol negatifse std::nullopt
    std::optional<double> time_at_distance(const MissileProfile& missile_profile, double distance_nm);
}
