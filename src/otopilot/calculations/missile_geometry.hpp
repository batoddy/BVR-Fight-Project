#pragma once

#include <optional>

#include "model/aircraft.hpp"
#include "model/missile.hpp"
#include "model/missile_profiles.hpp"

// Füze hesapları. Saf fonksiyonlar.
//   missile: atılmış füze (has_kinematics true olmalı), target: füzenin hedefi olan uçak
// Mesafe ve kapanma geometry'nin aynı fonksiyonlarıyla, füzenin Kinematics'i üzerinden bulunur.
// Uçuş uyumu (speed/distance/time): atılan füzenin hangi mesafede, hangi hızda, ne zaman olacağı.
// Süre boost bitişinden sayılır; A-pole, F-pole ve TR bunu kullanır (calculations/fuze_ucus_tablosu.md).
namespace missile_geometry
{
    double range_nm(const Missile& missile, const Aircraft& target);       // NM, füze ile hedef arası 3B mesafe
    double closure_kt(const Missile& missile, const Aircraft& target);     // kt, füze-hedef mesafesinin azalma hızı; + yaklaşıyor
    bool is_pitbull(const Missile& missile, double range_nm);              // hedefe pitbull_range_nm'den yakın; Fox1/Fox2'de (0) hep false

    double speed_at_time_kt(const MissileProfile& missile_profile, double t_s);                      // kt, t_s saniyedeki hız
    double distance_at_time_nm(const MissileProfile& missile_profile, double t_s);                   // NM, t_s saniyede alınan yol
    std::optional<double> time_at_distance_s(const MissileProfile& missile_profile, double distance_nm);   // s, yolu alma süresi; erişemez ya da yol negatifse yok

    std::optional<double> catch_time_left_s(const Missile& missile);       // s, hedefe yetişmesine kalan süre; yetişemezse yok. target.range/closure'dan sonra
    std::optional<double> catch_time_left_s(const Missile& missile, double closure_kt);   // aynısı, verilen kapanmayla (ör. hedef dönerse)
}
