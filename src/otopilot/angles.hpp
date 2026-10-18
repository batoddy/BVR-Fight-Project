#pragma once

// Açı sabitleri ve yön dönüşümleri.
//
// Python/JSBSim yönü 0..360 kullanır: 0 kuzey, 90 doğu, 180 güney, 270 batı.
// C++ içinde ±180 kullanırız:          0 kuzey, +90 doğu (sağ), -90 batı (sol), 180 güney.

#include <cmath>

inline constexpr double PI = 3.14159265358979323846;   // MinGW c++17'de M_PI yok
inline constexpr double DEG_TO_RAD = PI / 180.0;
inline constexpr double RAD_TO_DEG = 180.0 / PI;

// Herhangi bir açıyı (-180, 180] aralığına getirir.   270 -> -90,  -200 -> 160,  180 -> 180
// Okunan yönde de, hesaptaki açı farklarında da (ör. bearing - heading) bunu kullan.
inline double wrap_180(double deg)
{
    double a = std::fmod(deg, 360.0);   // sonuç (-360, 360) aralığında

    if (a > 180.0)
    {
        a -= 360.0;
    }
    if (a <= -180.0)
    {
        a += 360.0;
    }

    return a;
}

// Herhangi bir açıyı [0, 360) aralığına getirir.   -90 -> 270
// Yalnızca Python'a yön gönderirken kullanılır.
inline double wrap_360(double deg)
{
    double a = std::fmod(deg, 360.0);

    if (a < 0.0)
    {
        a += 360.0;
    }

    return a;
}
