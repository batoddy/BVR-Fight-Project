#pragma once

#include "model/aircraft.hpp"

// İki nesne arasındaki geometri. Hiçbir şey saklamaz; fonksiyonlar static.
//
// Girdi Kinematics: uçak da olabilir, ileride atılmış füze de.
//   own:    bakan taraf (hesap onun yerinden yapılır)
//   target: bakılan taraf
// Enlem/boylam WGS84 jeodezik derece, irtifa ft. Mesafe NM, hız kt döner.
class Geometry
{
public:
    // NM, iki nokta arası 2B (yatay) mesafe; irtifa farkı yok sayılır
    static double range_2d_nm(const Kinematics& own, const Kinematics& target);

    // NM, iki nokta arası 3B düz çizgi mesafe
    static double range_3d_nm(const Kinematics& own, const Kinematics& target);

    // derece ±180, own'dan target'a bakış yönü; 0 kuzey, + doğu
    static double bearing_deg(const Kinematics& own, const Kinematics& target);

    // derece ±180, ATA (Antenna Train Angle): own'un burnu ile target arasındaki açı
    //   0 target tam önümde, + sağımda, - solumda, ±180 arkamda. ATA = bearing - heading
    static double ata_deg(const Kinematics& own, const Kinematics& target);

    // derece ±180, aspect (burun referanslı): target'ın burnu ile target'tan own'a bakış arasındaki açı
    //   0 hot (burnu bize dönük), ±90 beam (yanımızdan geçiyor), 180 cold (kuyruğu bize dönük)
    //   + own target'ın sağında, - solunda. Aspect = bearing(target -> own) - target heading
    static double aspect_angle_deg(const Kinematics& own, const Kinematics& target);


    // NM, LOS (line of sight) vektörü: own'dan target'a kuzey, doğu, aşağı. Uzunluğu range_3d_nm.
    static Ned los_nm(const Kinematics& own, const Kinematics& target);

    // birimsiz, LOS birim vektörü: yalnız yön (uzunluğu 1). Bir hızın "düşmana doğru" bileşeni
    // için iç çarpımda kullanılır: v · los_unit
    static Ned los_unit(const Kinematics& own, const Kinematics& target);

    // kt, aramızdaki mesafenin azalma hızı (Vc); + yaklaşıyoruz. İkimizin hareketi de dahil.
    static double rate_of_closure_kt(const Kinematics& own, const Kinematics& target);

    // kt, yalnız target'ın own'a doğru hızı; + bana geliyor. WEZ hesabı bunu kullanır.
    static double enemy_closing_kt(const Kinematics& own, const Kinematics& target);
};
