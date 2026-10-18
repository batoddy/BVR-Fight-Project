#include "geometry.hpp"
#include <cmath>
#include "util_functions.hpp"

// Düz dünya yaklaşımı: iki uçak arasında Dünya düz kabul edilir. Hata yaklaşık %0,3
// (90 km'de ~300 m; ortalama yarıçap ve irtifanın yok sayılmasından). WEZ gibi km'lik
// sınırlar için yeterli. Daha hassası gerekirse util_functions.hpp'deki lla_to_ecef ile
// yapılır; fonksiyonların dışarıdan görünüşü değişmez.
static const double EARTH_RADIUS_M = 6371000.0;   // m, ortalama Dünya yarıçapı

// own'dan target'a kuzey ve doğu yönünde kaç metre. Yalnız bu dosyada (static).
static void horizontal_distance_m(const Kinematics& own, const Kinematics& target, double& north_m, double& east_m)
{
    // Boylam çizgileri kutuplara doğru daralır; doğu mesafesi ortalama enlemin cos'u ile küçülür.
    double mean_lat_rad = (own.lat_deg + target.lat_deg) / 2.0 * DEG_TO_RAD;

    north_m = (target.lat_deg - own.lat_deg) * DEG_TO_RAD * EARTH_RADIUS_M;
    east_m = (target.lon_deg - own.lon_deg) * DEG_TO_RAD * EARTH_RADIUS_M * std::cos(mean_lat_rad);
}

double Geometry::range_2d_m(const Kinematics& own, const Kinematics& target)
{
    double north_m = 0;
    double east_m  = 0;

    horizontal_distance_m(own, target, north_m, east_m);

    return std::sqrt(north_m * north_m + east_m * east_m);
}

double Geometry::range_3d_m(const Kinematics& own, const Kinematics& target)
{
    double horizontal_m = range_2d_m(own, target);
    double up_m = target.altitude_m - own.altitude_m;

    return std::sqrt(horizontal_m * horizontal_m + up_m * up_m);
}

double Geometry::bearing_deg(const Kinematics& own, const Kinematics& target)
{
    double north_m = 0;
    double east_m  = 0;

    horizontal_distance_m(own,target,north_m,east_m);

    // atan2(doğu, kuzey): açı kuzeyden saat yönünde ölçülür (0 kuzey, +90 doğu), sonuç radyan.
    // atan2 iki işarete de baktığı için düşmanın hangi çeyrekte olduğunu doğru bulur.
    double bearing_deg = wrap_180(atan2(east_m, north_m) * RAD_TO_DEG);

    return bearing_deg;
}

double Geometry::ATA_deg(const Kinematics& own, const Kinematics& target)
{
    // ATA = bearing - heading: kuzeye göre düşman yönünden kendi burun yönümü çıkarırım,
    // geriye burnuma göre açı kalır. Fark ±180'i aşabilir (170 - (-170) = 340); wrap_180 -20 yapar.
    double ata_angle_deg = wrap_180(own.heading_deg_180 - bearing_deg(own,target));

    return ata_angle_deg;
}
double Geometry::aspect_angle_deg(const Kinematics& own, const Kinematics& target)
{
    // Ters bakış: target'tan own'a. Argümanlar bilerek yer değiştirdi (target, own).
    double north_m = 0;
    double east_m  = 0;
    horizontal_distance_m(target, own, north_m, east_m);

    // Düşmandan bize bearing (kuzeye göre)
    double enemy_bearing_deg = wrap_180(atan2(east_m, north_m) * RAD_TO_DEG);

    // Düşmanın ATA'sı gibi: kendi burun yönünü çıkarır. Fark ±180'i aşabilir; wrap_180 ile sar.
    double aspect_angle_deg = enemy_bearing_deg - target.heading_deg_180;

    return aspect_angle_deg;
}
