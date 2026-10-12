#pragma once

// Açı sabitleri ve yön dönüşümleri.
//
// Python/JSBSim yönü 0..360 kullanır (0 kuzey, 90 doğu, 180 güney, 270 batı).
// C++ içinde ±180 kullanıyoruz:
//      0  kuzey
//    +90  sağ  (doğu)
//    -90  sol  (batı)
//    180  güney
// Çeviri yalnızca okurken (objects.cpp) ve komut yazarken yapılır;
// hesap kodunda hep ±180 görürsün.

#include <cmath>

// MinGW -std=c++17 ile M_PI tanımlı değil; kendi sabitimiz.
inline constexpr double PI = 3.14159265358979323846;
inline constexpr double DEG_TO_RAD = PI / 180.0;
inline constexpr double RAD_TO_DEG = 180.0 / PI;

// 0..360 -> ±180.   270 -> -90,   190 -> -170,   180 -> 180
inline double to_signed_heading(double heading_deg) 
{
    double h = std::fmod(heading_deg, 360.0);

    if (h > 180.0) 
    {
      h -= 360.0;
    }

    if (h <= -180.0)
    {
      h += 360.0;  
    }

    return h;
}

// ±180 -> 0..360 (Python'un beklediği).   -90 -> 270,   -170 -> 190
inline double to_compass_heading(double heading_deg)
 {
    double h = std::fmod(heading_deg, 360.0);
    
    if (h < 0.0)
    {
      h += 360.0;  
    }

    return h;
}
