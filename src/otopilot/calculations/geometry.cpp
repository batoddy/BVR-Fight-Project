#include "calculations/geometry.hpp"

#include <cmath>
#include <stdexcept>

#include "common/angles.hpp"
#include "common/units.hpp"
#include "model/picture.hpp"

namespace
{
    // Düz dünya yaklaşımı: iki uçak arasında Dünya düz kabul edilir. Hata yaklaşık %0,3
    // (90 km'de ~300 m; ortalama yarıçap ve irtifanın yok sayılmasından). WEZ gibi km'lik
    // sınırlar için yeterli. Daha hassası gerekirse WGS84 ECEF dönüşümüyle yapılır (eski common/geodesy.hpp
    // git geçmişinde); fonksiyonların dışarıdan görünüşü değişmez.
    constexpr double EARTH_RADIUS_NM = meter_to_nmile(6371000.0);
    constexpr double MIN_GUIDANCE_SPEED_KT = 1e-9;   // kt, yönü tanımsız sayılan sıfıra yakın hız
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

// los değişim hızı = v_dik / range
double geometry::los_rate_deg_s(const Kinematics& own, const Kinematics& other)
{
    const double range_nm = range_2d_nm(own, other);
    if (range_nm < meter_to_nmile(1.0))
    {
        return 0.0;   // aynı noktadalar: yön tanımsız
    }

    const Ned los = los_nm(own, other);
    const double v_north_kt = other.velocity_ned_kt.north - own.velocity_ned_kt.north;
    const double v_east_kt = other.velocity_ned_kt.east - own.velocity_ned_kt.east;

    const double v_dik_kt = (los.north * v_east_kt - los.east * v_north_kt) / range_nm;
    return v_dik_kt / range_nm / SECONDS_PER_HOUR * RAD_TO_DEG;
}

// LOS'un sağındaki yatay birim vektör (-sin bearing, cos bearing) ile hızın skaler çarpımı.
double geometry::lateral_speed_kt(double bearing_deg, const Ned& velocity_ned_kt)
{
    const double bearing_rad = bearing_deg * DEG_TO_RAD;
    return velocity_ned_kt.east * std::cos(bearing_rad) - velocity_ned_kt.north * std::sin(bearing_rad);
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

double geometry::ta_deg(const Kinematics& own, const Kinematics& other)
{
    // other'ın gözünden ATA: other'dan bana bearing − other'ın burun yönü. 0: burnu bana dönük (hot).
    const double other_to_own_bearing_deg = bearing_deg(other, own);
    return wrap_180(other_to_own_bearing_deg - other.heading_deg);
}

// Hedef nokta = düşman konumu + range_nm uzaklıkta hedef TA yönündeki vektör.
// İstenen hız = düşmanın yer hızı + (hedef nokta − kendi konumum) / tepki süresi.
double geometry::flank_heading_deg(const Kinematics& own, const Picture& picture, double target_ta_deg, double range_nm, double response_s)
{
    if (!std::isfinite(response_s) || response_s <= 0.0)
    {
        throw std::invalid_argument("Flank tepki suresi pozitif ve sonlu olmalidir");
    }
    const double enemy_heading_deg = wrap_180(picture.bearing_deg + 180.0 - picture.ta_deg);
    const double target_bearing_rad = (enemy_heading_deg + target_ta_deg) * DEG_TO_RAD;
    const double target_north_nm = picture.los_nm.north + range_nm * std::cos(target_bearing_rad);
    const double target_east_nm = picture.los_nm.east + range_nm * std::sin(target_bearing_rad);
    const double correction_north_kt = target_north_nm / response_s * SECONDS_PER_HOUR;
    const double correction_east_kt = target_east_nm / response_s * SECONDS_PER_HOUR;
    const double desired_north_kt = picture.enemy_velocity_ned_kt.north + correction_north_kt;
    const double desired_east_kt = picture.enemy_velocity_ned_kt.east + correction_east_kt;

    if (std::abs(desired_north_kt) < MIN_GUIDANCE_SPEED_KT && std::abs(desired_east_kt) < MIN_GUIDANCE_SPEED_KT)
    {
        return own.heading_deg;
    }
    return std::atan2(desired_east_kt, desired_north_kt) * RAD_TO_DEG;
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
