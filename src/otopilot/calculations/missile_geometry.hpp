#pragma once

#include <optional>

#include "model/aircraft.hpp"
#include "model/missile.hpp"
#include "model/missile_profiles.hpp"

// Füze hesapları. Saf fonksiyonlar.
//   missile: atılmış füze (has_kinematics true olmalı)
//   target:  füzenin hedefi olan uçak
// Mesafe ve kapanma geometry'nin aynı fonksiyonlarıyla, füzenin Kinematics'i üzerinden bulunur.
// Uçuş uyumu (speed_at_time_kt, distance_at_time_nm, time_at_distance_s): atılan füzenin hangi mesafede,
// hangi hızda, ne zaman olacağı. Süre boost bitişinden sayılır; A-pole, F-pole ve TR bunu kullanır.
namespace missile_geometry
{
    /// NM, füze ile hedef arası 3B mesafe
    double range_nm(const Missile& missile, const Aircraft& target);

    /// kt, füze ile hedef arası mesafenin azalma hızı; + yaklaşıyor
    double closure_kt(const Missile& missile, const Aircraft& target);

    /// Füze pitbull mı: hedefe pitbull_range_nm'den yakın. pitbull_range_nm 0 ise (Fox1, Fox2 tipleri) hep false.
    bool is_pitbull(const Missile& missile, double range_nm);

    /// kt, füzenin t_s saniyedeki hızı
    double speed_at_time_kt(const MissileProfile& missile_profile, double t_s);

    /// NM, füzenin t_s saniyede aldığı yol
    double distance_at_time_nm(const MissileProfile& missile_profile, double t_s);

    /// s, füzenin distance_nm yolu alması için gereken süre; füze o yola erişemezse veya yol negatifse std::nullopt
    std::optional<double> time_at_distance_s(const MissileProfile& missile_profile, double distance_nm);

    /// s, havadaki füzenin hedefine yetişmesine kalan süre; füze yetişemiyorsa std::nullopt.
    /// target.range_nm ve target.closure_kt'den sonra çağrılır.
    std::optional<double> catch_time_left_s(const Missile& missile);
}
