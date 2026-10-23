#include "calculations/geometry.hpp"
#include <cmath>
#include "common/util_functions.hpp"

// Düz dünya yaklaşımı: iki uçak arasında Dünya düz kabul edilir. Hata yaklaşık %0,3
// (90 km'de ~300 m; ortalama yarıçap ve irtifanın yok sayılmasından). WEZ gibi km'lik
// sınırlar için yeterli. Daha hassası gerekirse util_functions.hpp'deki lla_to_ecef ile
// yapılır; fonksiyonların dışarıdan görünüşü değişmez.
static constexpr double EARTH_RADIUS_NM = meter_to_nmile(6371000.0);

// own'dan target'a kuzey ve doğu yönünde kaç NM. Yalnız bu dosyada (static).
static void horizontal_distance_nm(const Kinematics& own, const Kinematics& target, double& north_nm, double& east_nm)
{
    // Boylam çizgileri kutuplara doğru daralır; doğu mesafesi ortalama enlemin cos'u ile küçülür.
    double mean_lat_rad = (own.lat_deg + target.lat_deg) / 2.0 * DEG_TO_RAD;

    north_nm = (target.lat_deg - own.lat_deg) * DEG_TO_RAD * EARTH_RADIUS_NM;
    east_nm = (target.lon_deg - own.lon_deg) * DEG_TO_RAD * EARTH_RADIUS_NM * std::cos(mean_lat_rad);
}

Ned Geometry::los_nm(const Kinematics& own, const Kinematics& target)
{
    Ned los_nm;
    horizontal_distance_nm(own, target, los_nm.north, los_nm.east);
    // Üç bileşen de NM olmalı: irtifa farkı C++ modelinde feet cinsindedir.
    los_nm.down = feet_to_nmile(own.altitude_ft - target.altitude_ft);

    return los_nm;
}

Ned Geometry::los_unit(const Kinematics& own, const Kinematics& target)
{
    Ned los = los_nm(own, target);
    double length = std::sqrt(los.north * los.north + los.east * los.east + los.down * los.down);

    if (length < meter_to_nmile(1.0))
    {
        return Ned{};   // aynı noktadalar: yön tanımsız, sıfır vektör
    }

    los.north /= length;
    los.east /= length;
    los.down /= length;
    return los;
}

double Geometry::range_2d_nm(const Kinematics& own, const Kinematics& target)
{
    Ned los = los_nm(own,target);
    return std::sqrt(los.north * los.north + los.east * los.east);
}

double Geometry::range_3d_nm(const Kinematics& own, const Kinematics& target)
{
    Ned los = los_nm(own,target);

    return std::sqrt(los.north*los.north + los.east*los.east + los.down*los.down);
}

double Geometry::bearing_deg(const Kinematics& own, const Kinematics& target)
{
    // atan2(doğu, kuzey): açı kuzeyden saat yönünde ölçülür (0 kuzey, +90 doğu), sonuç radyan.
    // atan2 iki işarete de baktığı için düşmanın hangi çeyrekte olduğunu doğru bulur.

    Ned los = los_nm(own,target);
    double bearing_deg = std::atan2(los.east, los.north) * RAD_TO_DEG;

    return bearing_deg;
}

double Geometry::ata_deg(const Kinematics& own, const Kinematics& target)
{
    // ATA = bearing - heading: kuzeye göre düşman yönünden kendi burun yönümü çıkarırım,
    // geriye burnuma göre açı kalır. Fark ±180'i aşabilir (170 - (-170) = 340); wrap_180 -20 yapar.
    double ata_angle_deg = wrap_180(bearing_deg(own,target) - own.heading_deg_180);

    return ata_angle_deg;
}
double Geometry::aspect_angle_deg(const Kinematics& own, const Kinematics& target)
{
    Ned los = los_nm(target,own);

    // Düşmandan bize bearing (kuzeye göre)
    double enemy_bearing_deg = atan2(los.east, los.north) * RAD_TO_DEG;

    // Düşmanın ATA'sı gibi: kendi burun yönünü çıkarır. Fark ±180'i aşabilir; wrap_180 ile sar.
    double aspect_angle_deg = wrap_180(enemy_bearing_deg - target.heading_deg_180);

    return aspect_angle_deg;
}

double Geometry::rate_of_closure_kt(const Kinematics& own, const Kinematics& target)
{
    // Ben düşmana doğru gittikçe ve düşman bana doğru geldikçe mesafe azalır.
    Ned u = los_unit(own,target);

    double own_v = dot(own.velocity_ned_kt,u);
    double target_v = dot(target.velocity_ned_kt,u);

    return own_v - target_v;
}

double Geometry::enemy_closing_kt(const Kinematics& own, const Kinematics& target)
{
    Ned u = los_unit(own, target);
    return -dot(target.velocity_ned_kt, u);   // düşman LOS'un tersine gidiyorsa bana geliyor: +
}
