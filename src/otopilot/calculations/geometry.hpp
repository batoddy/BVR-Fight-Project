#pragma once

#include "common/ned.hpp"
#include "model/kinematics.hpp"

// İki nesne arasındaki geometri. Saf fonksiyonlar.
//
// Girdi Kinematics: uçak da olabilir, atılmış füze de.
//   own:   bakan taraf (hesap onun yerinden yapılır)
//   other: bakılan taraf
// Enlem/boylam WGS84 jeodezik derece, irtifa ft. Mesafe NM, hız kt döner.
namespace geometry
{
    // NM, iki nokta arası 2B (yatay) mesafe; irtifa farkı yok sayılır
    double range_2d_nm(const Kinematics& own, const Kinematics& other);

    // NM, iki nokta arası 3B düz çizgi mesafe
    double range_3d_nm(const Kinematics& own, const Kinematics& other);

    // derece ±180, own'dan other'a bakış yönü; 0 kuzey, + doğu
    double bearing_deg(const Kinematics& own, const Kinematics& other);

    // derece ±180, ATA (Antenna Train Angle): own'un burnu ile other arasındaki açı
    //   0 tam önümde, + sağımda, - solumda, ±180 arkamda. ATA = bearing - heading
    double ata_deg(const Kinematics& own, const Kinematics& other);

    // derece ±180, aspect (burun referanslı): other'ın burnu ile other'dan own'a bakış arasındaki açı
    //   0 hot (burnu bize dönük), ±90 beam, 180 cold (kuyruğu bize dönük)
    //   + own other'ın sağında, - solunda. Aspect = bearing(other -> own) - other heading
    double aspect_angle_deg(const Kinematics& own, const Kinematics& other);

    // NM, LOS (line of sight) vektörü: own'dan other'a kuzey, doğu, aşağı. Uzunluğu range_3d_nm.
    Ned los_nm(const Kinematics& own, const Kinematics& other);

    // birimsiz, LOS birim vektörü: yalnız yön (uzunluğu 1). Aynı noktadalarsa sıfır vektör.
    // Bir hızın "other'a doğru" bileşeni için iç çarpımda kullanılır: v · los_unit
    Ned los_unit(const Kinematics& own, const Kinematics& other);

    // kt, aradaki mesafenin azalma hızı (Vc); + yaklaşıyoruz. İkimizin hareketi de dahil.
    double closure_kt(const Kinematics& own, const Kinematics& other);

    // kt, yalnız other'ın own'a doğru hızı; + bana geliyor. WEZ hesabı bunu kullanır.
    double other_closure_kt(const Kinematics& own, const Kinematics& other);
}
