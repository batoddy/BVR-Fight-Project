#include "geometry.hpp"
#include <cmath>
#include "angles.hpp"

// WGS84 enlem/boylam/irtifa -> ECEF.
//
// Formül:
//   φ = enlem (radyan),  λ = boylam (radyan),  h = irtifa (m)
//   a  = 6 378 137 m          ekvator yarıçapı
//   e² = 0,00669437999014     basıklık sabiti
//
//   N = a / √(1 − e² · sin²φ)        o enlemdeki yer yarıçapı
//
//   x = (N + h) · cos φ · cos λ
//   y = (N + h) · cos φ · sin λ
//   z = (N · (1 − e²) + h) · sin φ
Ecef to_ecef(const Kinematics& k)
{
    const double a = 6378137.0;           // WGS84 ekvator yarıçapı, m
    const double e2 = 6.69437999014e-3;   // WGS84 basıklık sabiti (eksantrikliğin karesi)

    double lat = k.lat_deg * DEG_TO_RAD;
    double lon = k.lon_deg * DEG_TO_RAD;
    double h = k.alt_msl_m;

    double sin_lat = std::sin(lat);
    double cos_lat = std::cos(lat);
    double sin_lon = std::sin(lon);
    double cos_lon = std::cos(lon);

    // N: o enlemdeki yer yarıçapı. Dünya basık olduğu için enleme göre değişir.
    double n = a / std::sqrt(1.0 - e2 * sin_lat * sin_lat);

    Ecef p;
    p.x = (n + h) * cos_lat * cos_lon;
    p.y = (n + h) * cos_lat * sin_lon;
    p.z = (n * (1.0 - e2) + h) * sin_lat;
    return p;
}

// Range: iki uçak arasındaki 3B düz çizgi mesafesi, m.
//
// Formül:
//   Δx = x_hedef − x_biz
//   Δy = y_hedef − y_biz
//   Δz = z_hedef − z_biz
//
//   R = √(Δx² + Δy² + Δz²)
double range_m(const Kinematics& own, const Kinematics& target)
{
    Ecef p = to_ecef(own);
    Ecef q = to_ecef(target);

    double dx = q.x - p.x;
    double dy = q.y - p.y;
    double dz = q.z - p.z;

    double distance_squared = dx * dx + dy * dy + dz * dz;
    return std::sqrt(distance_squared);
}

// LOS (line of sight): hedef bizden ne kadar kuzeyde, doğuda, aşağıda.
//
// Formül: ECEF farkını bizim noktamızdaki yerel eksenlere döndürmek.
//   φ, λ = bizim enlem ve boylamımız (radyan)
//   Δx, Δy, Δz = hedefin ECEF'i − bizim ECEF'imiz
//
//   North = −sin φ · cos λ · Δx  −  sin φ · sin λ · Δy  +  cos φ · Δz
//   East  = −sin λ · Δx          +  cos λ · Δy
//   Down  = −cos φ · cos λ · Δx  −  cos φ · sin λ · Δy  −  sin φ · Δz
Ned line_of_sight(const Kinematics& own, const Kinematics& target)
{
    Ecef p = to_ecef(own);
    Ecef q = to_ecef(target);

    double dx = q.x - p.x;
    double dy = q.y - p.y;
    double dz = q.z - p.z;

    double lat = own.lat_deg * DEG_TO_RAD;
    double lon = own.lon_deg * DEG_TO_RAD;

    double sin_lat = std::sin(lat);
    double cos_lat = std::cos(lat);
    double sin_lon = std::sin(lon);
    double cos_lon = std::cos(lon);

    Ned los;
    los.north = -sin_lat * cos_lon * dx - sin_lat * sin_lon * dy + cos_lat * dz;
    los.east = -sin_lon * dx + cos_lon * dy;
    los.down = -cos_lat * cos_lon * dx - cos_lat * sin_lon * dy - sin_lat * dz;
    return los;
}

// Yatay mesafe: irtifa farkını saymadan, haritada ölçülen mesafe, m.
//
// Formül (Pisagor, yalnızca kuzey ve doğu bileşenleri):
//   R_yatay = √(North² + East²)
double horizontal_range_m(const Kinematics& own, const Kinematics& target)
{
    Ned los = line_of_sight(own, target);

    double distance_squared = los.north * los.north + los.east * los.east;
    return std::sqrt(distance_squared);
}

double bearing_deg(const Kinematics& own, const Kinematics& target)
{
    Ned los = line_of_sight(own,target);

    double bearing_rad = std::atan2(los.east, los.north);

    return bearing_rad * RAD_TO_DEG;
}
