#pragma once

#include "aircraft.hpp"

// İki nesne arasındaki geometri. Hiçbir şey saklamaz; fonksiyonlar static.
//
// Girdi Kinematics: uçak da olabilir, ileride atılmış füze de.
//   own:    bakan taraf (hesap onun yerinden yapılır)
//   target: bakılan taraf
// Enlem/boylam WGS84 jeodezik derece, irtifa MSL metre.
class Geometry
{
public:
    // m, iki nokta arası 3B düz çizgi mesafe
    static double range_m(const Kinematics& own, const Kinematics& target);

    // derece ±180, own'dan target'a bakış yönü; 0 kuzey, + doğu
    static double bearing_deg_180(const Kinematics& own, const Kinematics& target);
};
