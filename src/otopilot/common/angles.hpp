#pragma once

// Açı sarma. Python/JSBSim yönü 0..360 kullanır: 0 kuzey, 90 doğu, 180 güney, 270 batı.
// C++ içinde ±180 kullanılır:              0 kuzey, +90 doğu (sağ), -90 batı (sol), 180 güney.

#include <cmath>

inline double wrap_180(double deg)   // (-180, 180]: 270 → -90, -200 → 160. Okunan yönde ve açı farklarında (bearing − heading)
{
    double wrapped = std::fmod(deg, 360.0);   // sonuç (-360, 360) aralığında

    if (wrapped > 180.0)
    {
        wrapped -= 360.0;
    }
    if (wrapped <= -180.0)
    {
        wrapped += 360.0;
    }

    return wrapped;
}

inline double wrap_360(double deg)   // [0, 360): -90 → 270. Yalnız Python'a yön gönderirken
{
    double wrapped = std::fmod(deg, 360.0);

    if (wrapped < 0.0)
    {
        wrapped += 360.0;
    }

    return wrapped;
}
