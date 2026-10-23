#pragma once

// Genel dönüşümler: açı sabitleri, yön sarma, LLA <-> ECEF.
// Uçak ya da Kinematics bilmez; yalnız sayıları çevirir. İki nesne arası hesap Geometry'de.
//
// Python/JSBSim yönü 0..360 kullanır: 0 kuzey, 90 doğu, 180 güney, 270 batı.
// C++ içinde ±180 kullanırız:          0 kuzey, +90 doğu (sağ), -90 batı (sol), 180 güney.

#include <cmath>

// ============================================================================
// BİRİMLER — C++ içinde irtifa ft, mesafe NM, hız kt, süre saniye.
// Python uçuş mesajları metre/m/s kullanır; dönüşüm IO sınırında yapılır.
// ============================================================================

inline constexpr double NMILE_TO_METER = 1852.0;
inline constexpr double METER_TO_NMILE = 1.0 / NMILE_TO_METER;
inline constexpr double FOOT_TO_METER = 0.3048;
inline constexpr double SECONDS_PER_HOUR = 3600.0;

inline constexpr double meter_to_nmile(double m) { return m * METER_TO_NMILE; }
inline constexpr double nmile_to_meter(double nm) { return nm * NMILE_TO_METER; }
inline constexpr double meter_to_feet(double m) { return m / FOOT_TO_METER; }
inline constexpr double feet_to_meter(double ft) { return ft * FOOT_TO_METER; }
inline constexpr double feet_to_nmile(double ft) { return meter_to_nmile(feet_to_meter(ft)); }
inline constexpr double nmile_to_feet(double nm) { return meter_to_feet(nmile_to_meter(nm)); }
inline constexpr double mps_to_knot(double mps) { return meter_to_nmile(mps) * SECONDS_PER_HOUR; }
inline constexpr double knot_to_mps(double kt) { return nmile_to_meter(kt) / SECONDS_PER_HOUR; }
inline constexpr double fps_to_knot(double fps) { return mps_to_knot(feet_to_meter(fps)); }
inline constexpr double knot_to_fps(double kt) { return meter_to_feet(knot_to_mps(kt)); }

// Yerel eksenler (NED: North, East, Down), bakan noktaya göre.
// Konum farkı NM, hız kt, normalize yön ise birimsizdir; alan adı birimi belirtir.
//   north: kuzeye, east: doğuya, down: aşağıya (hedef bizden alçaktaysa +)
struct Ned
{
    double north = 0;
    double east = 0;
    double down = 0;
};

struct Lla
{
    double lat_deg = 0;      // derece, jeodezik enlem
    double lon_deg = 0;      // derece, boylam
    double altitude_ft = 0;   // ft
};

struct Ecef
{
    double x_nm = 0;
    double y_nm = 0;
    double z_nm = 0;
};

inline constexpr double GRAVITY_MPS2 = 9.81;
inline constexpr double PI = 3.14159265358979323846;   // MinGW c++17'de M_PI yok
inline constexpr double DEG_TO_RAD = PI / 180.0;
inline constexpr double RAD_TO_DEG = 180.0 / PI;

inline constexpr double WGS84_A_NM = meter_to_nmile(6378137.0); // NM, ekvator yarıçapı
inline constexpr double WGS84_E2 = 6.69437999014e-3;     // birinci basıklık (eksantriklik) karesi

// İç çarpım: a'nın b yönündeki bileşeni (b birim vektörse). Ör. hız · los_unit = hızın düşmana doğru kısmı.
// Vektörün diğer vektör üzerindeki büyüklüğü
inline double dot(const Ned& a, const Ned& b)
{
    return a.north * b.north + a.east * b.east + a.down * b.down;
}


// ---------------------------------------------------------------------------
// LLA <-> ECEF (WGS84)
//
// LLA:  Latitude, Longitude, Altitude: enlem, boylam (derece), irtifa (ft).
// ECEF: Earth-Centered, Earth-Fixed: Dünya merkezinde başlayan eksenler (NM).
//       x: ekvator ve 0 boylam yönü, y: ekvator ve 90 doğu yönü, z: kuzey kutbu yönü.
//
// Formül elipsoid yüksekliği bekler; altitude_ft feet cinsindedir.
// Feet/NM dönüşümü irtifa referansını değiştirmez, jeoit düzeltmesi uygulamaz.
// ---------------------------------------------------------------------------

// Enlem, boylam, irtifa -> ECEF. Kullanım: lla_to_ecef(k.lat_deg, k.lon_deg, k.altitude_ft)
inline Ecef lla_to_ecef(double lat_deg, double lon_deg, double altitude_ft)
{
    double lat = lat_deg * DEG_TO_RAD;
    double lon = lon_deg * DEG_TO_RAD;
    double altitude_nm = feet_to_nmile(altitude_ft);

    // N: o enlemde Dünya'nın eğrilik yarıçapı (kutuplara doğru büyür)
    double sin_lat = std::sin(lat);
    double n = WGS84_A_NM / std::sqrt(1.0 - WGS84_E2 * sin_lat * sin_lat);

    Ecef p;
    p.x_nm = (n + altitude_nm) * std::cos(lat) * std::cos(lon);
    p.y_nm = (n + altitude_nm) * std::cos(lat) * std::sin(lon);
    p.z_nm = (n * (1.0 - WGS84_E2) + altitude_nm) * sin_lat;
    return p;
}

// ECEF -> enlem, boylam, irtifa. Enlemin kapalı formülü yok; tahminle başlayıp
// birkaç kez düzeltilir. 5 tur milimetre doğruluğu için fazlasıyla yeterli.
inline Lla ecef_to_lla(const Ecef& p)
{
    double lon = std::atan2(p.y_nm, p.x_nm);
    double r = std::sqrt(p.x_nm * p.x_nm + p.y_nm * p.y_nm);   // NM, z eksenine uzaklık

    double lat = std::atan2(p.z_nm, r * (1.0 - WGS84_E2));  // ilk tahmin
    double altitude = 0.0;

    for (int i = 0; i < 5; i++)
    {
        double sin_lat = std::sin(lat);
        double n = WGS84_A_NM / std::sqrt(1.0 - WGS84_E2 * sin_lat * sin_lat);
        altitude = r / std::cos(lat) - n;
        lat = std::atan2(p.z_nm, r * (1.0 - WGS84_E2 * n / (n + altitude)));
    }

    Lla result;
    result.lat_deg = lat * RAD_TO_DEG;
    result.lon_deg = lon * RAD_TO_DEG;
    result.altitude_ft = nmile_to_feet(altitude);
    return result;
}


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
