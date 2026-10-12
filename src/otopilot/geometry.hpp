#pragma once

// İki uçak arasındaki geometri: WGS84 -> ECEF dönüşümü, range ve LOS.
//
// Enlem/boylam WGS84 jeodezik (state içindeki lat_deg zaten jeodezik gelir),
// irtifa metre. Sonuçlar metre.
//
// Burada yalnızca tipler ve fonksiyon bildirimleri var.
// Hesapların kendisi ve formülleri geometry.cpp içinde.

#include "objects.hpp"

// Dünya merkezli koordinat (ECEF), metre.
//   x: Dünya merkezinden ekvator ile 0° boylamın kesiştiği noktaya
//   y: Dünya merkezinden ekvator ile 90° doğu boylamının kesiştiği noktaya
//   z: Dünya merkezinden kuzey kutbuna
struct Ecef
{
    double x = 0;
    double y = 0;
    double z = 0;
};

// Bizim bulunduğumuz noktaya göre yerel eksenler, metre.
//   north: kuzeye doğru
//   east:  doğuya doğru
//   down:  yere doğru (hedef bizden alçaktaysa pozitif)
struct Ned
{
    double north = 0;
    double east = 0;
    double down = 0;
};

// WGS84 enlem/boylam/irtifa -> ECEF (x, y, z metre).
Ecef to_ecef(const Kinematics& k);

// Range: iki uçak arasındaki 3B düz çizgi mesafesi, m.
double range_m(const Kinematics& own, const Kinematics& target);

// LOS (line of sight): hedef bizden ne kadar kuzeyde, doğuda, aşağıda; m.
// Bearing ve ATA gibi açıların temeli bu vektördür.
Ned line_of_sight(const Kinematics& own, const Kinematics& target);

// Yatay mesafe: irtifa farkını saymadan, haritada ölçülen mesafe, m.
double horizontal_range_m(const Kinematics& own, const Kinematics& target);

// ATA (antenna train angle): burnumuz ile hedefe bakış çizgisi arasındaki açı.
double bearing_deg(const Kinematics& own, const Kinematics& target);
