#pragma once

// Açı sarma. Python/JSBSim yönü 0..360 kullanır: 0 kuzey, 90 doğu, 180 güney, 270 batı.
// C++ içinde ±180 kullanılır:              0 kuzey, +90 doğu (sağ), -90 batı (sol), 180 güney.

#include <cmath>

// Herhangi bir açıyı (-180, 180] aralığına getirir.   270 -> -90,  -200 -> 160,  180 -> 180
// Okunan yönde de, hesaptaki açı farklarında da (ör. bearing - heading) bunu kullan.
inline double wrap_180(double deg)
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

// Herhangi bir açıyı [0, 360) aralığına getirir.   -90 -> 270
// Yalnızca Python'a yön gönderirken kullanılır.
inline double wrap_360(double deg)
{
    double wrapped = std::fmod(deg, 360.0);

    if (wrapped < 0.0)
    {
        wrapped += 360.0;
    }

    return wrapped;
}
