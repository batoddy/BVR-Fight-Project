#include "calculations/geometry.hpp"

#include <cmath>

#include "common/angles.hpp"
#include "common/units.hpp"

namespace
{
    // Düz dünya yaklaşımı: iki uçak arasında Dünya düz kabul edilir. Hata yaklaşık %0,3
    // (90 km'de ~300 m; ortalama yarıçap ve irtifanın yok sayılmasından). WEZ gibi km'lik
    // sınırlar için yeterli. Daha hassası gerekirse common/geodesy.hpp'deki lla_to_ecef ile
    // yapılır; fonksiyonların dışarıdan görünüşü değişmez.
    constexpr double EARTH_RADIUS_NM = meter_to_nmile(6371000.0);
}

// own'dan other'a vektör, NM: kuzey, doğu, aşağı.
//   kuzey = enlem farkı (radyan) × Dünya yarıçapı
//   doğu  = boylam farkı (radyan) × Dünya yarıçapı × cos(ortalama enlem)
//           Boylam çizgileri kutuplara doğru daralır; aynı boylam farkı kuzeyde daha kısa bir mesafedir.
//   aşağı = irtifa farkı; other benden alçaksa +
Ned geometry::los_nm(const Kinematics& own, const Kinematics& other)
{
    const double mean_lat_rad = (own.lat_deg + other.lat_deg) / 2.0 * DEG_TO_RAD;

    Ned los;
    los.north = (other.lat_deg - own.lat_deg) * DEG_TO_RAD * EARTH_RADIUS_NM;
    los.east = (other.lon_deg - own.lon_deg) * DEG_TO_RAD * EARTH_RADIUS_NM * std::cos(mean_lat_rad);
    los.down = feet_to_nmile(own.altitude_ft - other.altitude_ft);
    return los;
}

// Aynı vektörün yönü (uzunluğu 1). Uzunluk, Pisagor: √(kuzey² + doğu² + aşağı²)
Ned geometry::los_unit(const Kinematics& own, const Kinematics& other)
{
    Ned los = los_nm(own, other);
    const double length = std::sqrt(los.north * los.north + los.east * los.east + los.down * los.down);

    if (length < meter_to_nmile(1.0))
    {
        return Ned{};   // aynı noktadalar: yön tanımsız, sıfır vektör
    }

    los.north /= length;
    los.east /= length;
    los.down /= length;
    return los;
}

// Yatay mesafe, Pisagor: √(kuzey² + doğu²)
double geometry::range_2d_nm(const Kinematics& own, const Kinematics& other)
{
    const Ned los = los_nm(own, other);
    return std::sqrt(los.north * los.north + los.east * los.east);
}

// 3B mesafe, Pisagor: √(kuzey² + doğu² + aşağı²)
double geometry::range_3d_nm(const Kinematics& own, const Kinematics& other)
{
    const Ned los = los_nm(own, other);
    return std::sqrt(los.north * los.north + los.east * los.east + los.down * los.down);
}

double geometry::bearing_deg(const Kinematics& own, const Kinematics& other)
{
    // atan2(doğu, kuzey): açı kuzeyden saat yönünde ölçülür (0 kuzey, +90 doğu).
    // atan2 iki işarete de baktığı için hangi çeyrekte olduğunu doğru bulur.
    const Ned los = los_nm(own, other);
    return std::atan2(los.east, los.north) * RAD_TO_DEG;
}

double geometry::ata_deg(const Kinematics& own, const Kinematics& other)
{
    // ATA = bearing - heading: kuzeye göre yönden kendi burun yönümü çıkarırım, burnuma göre açı kalır.
    // Fark ±180'i aşabilir (170 - (-170) = 340); wrap_180 -20 yapar.
    return wrap_180(bearing_deg(own, other) - own.heading_deg);
}

double geometry::aspect_angle_deg(const Kinematics& own, const Kinematics& other)
{
    // other'ın gözünden ATA: other'dan bana bearing − other'ın burun yönü. 0: burnu bana dönük (hot).
    const double other_to_own_bearing_deg = bearing_deg(other, own);
    return wrap_180(other_to_own_bearing_deg - other.heading_deg);
}

double geometry::closure_kt(const Kinematics& own, const Kinematics& other)
{
    // Ben other'a doğru gittikçe ve other bana doğru geldikçe mesafe azalır.
    const Ned unit = los_unit(own, other);
    return dot(own.velocity_ned_kt, unit) - dot(other.velocity_ned_kt, unit);
}

double geometry::other_closure_kt(const Kinematics& own, const Kinematics& other)
{
    const Ned unit = los_unit(own, other);
    return -dot(other.velocity_ned_kt, unit);   // other LOS'un tersine gidiyorsa bana geliyor: +
}
