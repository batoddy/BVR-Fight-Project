#pragma once

// Genel dönüşümler: açı sabitleri, yön sarma, LLA <-> ECEF.
// Uçak ya da Kinematics bilmez; yalnız sayıları çevirir. İki nesne arası hesap Geometry'de.
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

// ================ Nautical mile & meter conversions ==============

inline constexpr double NMILE_TO_METER = 1852;
inline constexpr double METER_TO_NMILE = 1.0 / 1852.0;


inline double nmile_to_meter(double nmile)
{
    return (nmile * NMILE_TO_METER);
}

inline double meter_to_nmile(double meter)
{
    return (meter * METER_TO_NMILE);
}


// ---------------------------------------------------------------------------
// LLA <-> ECEF (WGS84)
//
// LLA:  Latitude, Longitude, Altitude: enlem, boylam (derece), irtifa (m).
// ECEF: Earth-Centered, Earth-Fixed: Dünya merkezinde başlayan eksenler (m).
//       x: ekvator ve 0 boylam yönü, y: ekvator ve 90 doğu yönü, z: kuzey kutbu yönü.
//
// Formüldeki irtifa elipsoide göredir; bizdeki altitude_m deniz seviyesine (MSL) göre.
// Fark bir bölgede onlarca metre olabilir ama iki yakın uçakta neredeyse aynıdır;
// mesafe ve açı hesabında birbirini götürür. Bu yüzden altitude_m doğrudan verilir.
// ---------------------------------------------------------------------------

constexpr double WGS84_A_M = 6378137.0;           // m, ekvator yarıçapı
constexpr double WGS84_E2 = 6.69437999014e-3;     // birinci basıklık (eksantriklik) karesi

struct Lla
{
    double lat_deg = 0;      // derece, jeodezik enlem
    double lon_deg = 0;      // derece, boylam
    double altitude_m = 0;   // m
};

struct Ecef
{
    double x_m = 0;
    double y_m = 0;
    double z_m = 0;
};

// Enlem, boylam, irtifa -> ECEF. Kullanım: lla_to_ecef(k.lat_deg, k.lon_deg, k.altitude_m)
inline Ecef lla_to_ecef(double lat_deg, double lon_deg, double altitude_m)
{
    double lat = lat_deg * DEG_TO_RAD;
    double lon = lon_deg * DEG_TO_RAD;

    // N: o enlemde Dünya'nın eğrilik yarıçapı (kutuplara doğru büyür)
    double sin_lat = std::sin(lat);
    double n = WGS84_A_M / std::sqrt(1.0 - WGS84_E2 * sin_lat * sin_lat);

    Ecef p;
    p.x_m = (n + altitude_m) * std::cos(lat) * std::cos(lon);
    p.y_m = (n + altitude_m) * std::cos(lat) * std::sin(lon);
    p.z_m = (n * (1.0 - WGS84_E2) + altitude_m) * sin_lat;
    return p;
}

// ECEF -> enlem, boylam, irtifa. Enlemin kapalı formülü yok; tahminle başlayıp
// birkaç kez düzeltilir. 5 tur milimetre doğruluğu için fazlasıyla yeterli.
inline Lla ecef_to_lla(const Ecef& p)
{
    double lon = std::atan2(p.y_m, p.x_m);
    double r = std::sqrt(p.x_m * p.x_m + p.y_m * p.y_m);   // m, z eksenine uzaklık

    double lat = std::atan2(p.z_m, r * (1.0 - WGS84_E2));  // ilk tahmin
    double altitude = 0.0;

    for (int i = 0; i < 5; i++)
    {
        double sin_lat = std::sin(lat);
        double n = WGS84_A_M / std::sqrt(1.0 - WGS84_E2 * sin_lat * sin_lat);
        altitude = r / std::cos(lat) - n;
        lat = std::atan2(p.z_m, r * (1.0 - WGS84_E2 * n / (n + altitude)));
    }

    Lla result;
    result.lat_deg = lat * RAD_TO_DEG;
    result.lon_deg = lon * RAD_TO_DEG;
    result.altitude_m = altitude;
    return result;
}