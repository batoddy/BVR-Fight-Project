#pragma once

#include <cmath>
#include <optional>

#include "common/units.hpp"

// Genel hareket formülleri. Birimler units.hpp'deki gibi: mesafe NM, hız kt, ivme kt/s, süre s.
// kt "saatte NM" olduğu için süre saniyeden saate çevrilir (SECONDS_PER_HOUR).
//   sabit hız:   yol = hız × süre
//   sabit ivme:  yol x = v0·t + a·t²/2,  süre t = (−v0 + √(v0² + 2·a·x)) / a

inline constexpr double travel_distance_nm(double speed_kt, double time_s) { return speed_kt * time_s / SECONDS_PER_HOUR; }    // NM
inline constexpr double travel_time_s(double distance_nm, double speed_kt) { return SECONDS_PER_HOUR * distance_nm / speed_kt; } // s

inline constexpr double accel_travel_distance_nm(double start_kt, double accel_kt_s, double time_s)   // NM; accel_kt_s negatifse yavaşlama
{
    return (start_kt * time_s + 0.5 * accel_kt_s * time_s * time_s) / SECONDS_PER_HOUR;
}

inline std::optional<double> accel_travel_time_s(double distance_nm, double start_kt, double accel_kt_s)   // s; hız sıfıra inmeden erişilemezse yok
{
    if (accel_kt_s == 0.0)
    {
        return travel_time_s(distance_nm, start_kt);   // ivme yok: sabit hız
    }

    const double distance_kt_s = distance_nm * SECONDS_PER_HOUR;
    const double discriminant = start_kt * start_kt + 2.0 * accel_kt_s * distance_kt_s;
    if (discriminant < 0.0)
    {
        return std::nullopt;                           // kök içi negatif: o yola erişilemez
    }
    return (-start_kt + std::sqrt(discriminant)) / accel_kt_s;
}
