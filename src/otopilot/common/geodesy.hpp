#pragma once

// LLA <-> ECEF (WGS84).
//
// LLA:  Latitude, Longitude, Altitude: enlem, boylam (derece), irtifa (ft).
// ECEF: Earth-Centered, Earth-Fixed: Dünya merkezinde başlayan eksenler (NM).
//       x: ekvator ve 0 boylam yönü, y: ekvator ve 90 doğu yönü, z: kuzey kutbu yönü.
//
// Formül elipsoid yüksekliği bekler; altitude_ft feet cinsindedir.
// Feet/NM dönüşümü irtifa referansını değiştirmez, jeoit düzeltmesi uygulamaz.

#include <cmath>

#include "common/units.hpp"

struct Lla
{
    double lat_deg = 0;       // derece, jeodezik enlem
    double lon_deg = 0;       // derece, boylam
    double altitude_ft = 0;   // ft
};

struct Ecef
{
    double x_nm = 0;
    double y_nm = 0;
    double z_nm = 0;
};

inline constexpr double WGS84_A_NM = meter_to_nmile(6378137.0);   // NM, ekvator yarıçapı
inline constexpr double WGS84_E2 = 6.69437999014e-3;              // birinci basıklık (eksantriklik) karesi

// Enlem, boylam, irtifa -> ECEF. Kullanım: lla_to_ecef(k.lat_deg, k.lon_deg, k.altitude_ft)
inline Ecef lla_to_ecef(double lat_deg, double lon_deg, double altitude_ft)
{
    const double lat = lat_deg * DEG_TO_RAD;
    const double lon = lon_deg * DEG_TO_RAD;
    const double altitude_nm = feet_to_nmile(altitude_ft);

    // N: o enlemde Dünya'nın eğrilik yarıçapı (kutuplara doğru büyür)
    const double sin_lat = std::sin(lat);
    const double n = WGS84_A_NM / std::sqrt(1.0 - WGS84_E2 * sin_lat * sin_lat);

    Ecef point;
    point.x_nm = (n + altitude_nm) * std::cos(lat) * std::cos(lon);
    point.y_nm = (n + altitude_nm) * std::cos(lat) * std::sin(lon);
    point.z_nm = (n * (1.0 - WGS84_E2) + altitude_nm) * sin_lat;
    return point;
}

// ECEF -> enlem, boylam, irtifa. Enlemin kapalı formülü yok; tahminle başlayıp
// birkaç kez düzeltilir. 5 tur milimetre doğruluğu için fazlasıyla yeterli.
inline Lla ecef_to_lla(const Ecef& point)
{
    const double lon = std::atan2(point.y_nm, point.x_nm);
    const double axis_distance_nm = std::sqrt(point.x_nm * point.x_nm + point.y_nm * point.y_nm);   // z eksenine uzaklık

    double lat = std::atan2(point.z_nm, axis_distance_nm * (1.0 - WGS84_E2));   // ilk tahmin
    double altitude_nm = 0.0;

    for (int i = 0; i < 5; i++)
    {
        const double sin_lat = std::sin(lat);
        const double n = WGS84_A_NM / std::sqrt(1.0 - WGS84_E2 * sin_lat * sin_lat);
        altitude_nm = axis_distance_nm / std::cos(lat) - n;
        lat = std::atan2(point.z_nm, axis_distance_nm * (1.0 - WGS84_E2 * n / (n + altitude_nm)));
    }

    Lla result;
    result.lat_deg = lat * RAD_TO_DEG;
    result.lon_deg = lon * RAD_TO_DEG;
    result.altitude_ft = nmile_to_feet(altitude_nm);
    return result;
}
