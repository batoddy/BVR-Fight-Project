#pragma once

#include "common/ned.hpp"
#include "model/kinematics.hpp"

// İki nesne arasındaki geometri. Saf fonksiyonlar.
// Girdi Kinematics: uçak da olabilir, atılmış füze de. own: bakan taraf, other: bakılan taraf.
// Enlem/boylam WGS84 jeodezik derece, irtifa ft. Mesafe NM, hız kt döner.
//   ATA = bearing − own heading:              0 tam önümde, + sağımda, − solumda, ±180 arkamda
//   TA  = bearing(other → own) − other heading: 0 hot (burnu bize dönük), ±90 beam, 180 cold; + own other'ın sağında
namespace geometry
{
    double range_2d_nm(const Kinematics& own, const Kinematics& other);       // NM, yatay mesafe; irtifa farkı yok sayılır
    double range_3d_nm(const Kinematics& own, const Kinematics& other);       // NM, 3B düz çizgi mesafe
    double bearing_deg(const Kinematics& own, const Kinematics& other);       // derece ±180, own'dan other'a yön; 0 kuzey, + doğu
    double ata_deg(const Kinematics& own, const Kinematics& other);           // derece ±180, ATA: own'un burnu ile other arası
    double ta_deg(const Kinematics& own, const Kinematics& other);            // derece ±180, TA: other'ın burnu ile own'a bakışı arası
    Ned los_nm(const Kinematics& own, const Kinematics& other);               // NM, own'dan other'a vektör (kuzey, doğu, aşağı); uzunluğu range_3d_nm
    Ned los_unit(const Kinematics& own, const Kinematics& other);             // birimsiz, LOS yönü; aynı noktadalarsa sıfır. "other'a doğru" bileşen: v · los_unit
    double closure_kt(const Kinematics& own, const Kinematics& other);        // kt, aradaki mesafenin azalma hızı; + yaklaşıyoruz, ikimizin hareketi
    double other_closure_kt(const Kinematics& own, const Kinematics& other);  // kt, yalnız other'ın own'a doğru hızı; + bana geliyor
    double los_rate_deg_s(const Kinematics& own, const Kinematics& other);    // derece/s, bearing'in yatay değişimi; + saat yönünde; aynı noktada 0
}
