#include "calculations/turn.hpp"

#include <cmath>

#include "common/angles.hpp"
#include "common/units.hpp"

// Dönüş yarıçapı, seviye dönüşte:
//   Kanatlar n·g kaldırma üretir. Bunun dikey parçası ağırlığı (1 g) taşır,
//   yatay parçası uçağı döndürür:  yatay ivme = g · √(n² − 1)
//   Çembersel harekette ivme = V² / r  olduğundan  r = V² / yatay ivme
double turn::radius_m(double speed_kt, double load_factor_g)
{
    const double speed_mps = knot_to_mps(speed_kt);
    const double n = load_factor_g;

    const double turning_accel_mps2 = GRAVITY_MPS2 * std::sqrt(n * n - 1.0);
    return speed_mps * speed_mps / turning_accel_mps2;
}

// Kısa taraftan: fark ±180'e sarılır. + ise sağa (saat yönü), − ise sola.
double turn::angle_deg(double start_heading_deg, double target_heading_deg)
{
    return wrap_180(target_heading_deg - start_heading_deg);
}

double turn::average_speed_kt(double start_speed_kt, double end_speed_kt)
{
    return (start_speed_kt + end_speed_kt) / 2.0;
}

// Süre = yay uzunluğu / hız,  yay uzunluğu = açı (radyan) × yarıçap. Açının yönü değil büyüklüğü kullanılır.
double turn::time_s(double turn_deg, double radius_m, double speed_kt)
{
    const double turn_size_rad = std::abs(turn_deg) * DEG_TO_RAD;
    const double arc_length_m = turn_size_rad * radius_m;
    return arc_length_m / knot_to_mps(speed_kt);
}

// Uçak, dönüş merkezi etrafında bir yay çizer.
//   Sağa dönüşte merkez burnun 90° sağında, sola dönüşte 90° solundadır.
//   kuzey = s · r · (sin ψ_bitiş − sin ψ_başlangıç)
//   doğu  = s · r · (cos ψ_başlangıç − cos ψ_bitiş)        s: sağa +1, sola −1
Ned turn::displacement_nm(double start_heading_deg, double turn_deg, double radius_m)
{
    double side = 1.0;   // sağa
    if (turn_deg < 0.0)
    {
        side = -1.0;     // sola
    }

    const double end_heading_deg = wrap_180(start_heading_deg + turn_deg);
    const double start_heading_rad = start_heading_deg * DEG_TO_RAD;
    const double end_heading_rad = end_heading_deg * DEG_TO_RAD;
    const double radius_nm = meter_to_nmile(radius_m);

    Ned displacement;
    displacement.north = side * radius_nm * (std::sin(end_heading_rad) - std::sin(start_heading_rad));
    displacement.east = side * radius_nm * (std::cos(start_heading_rad) - std::cos(end_heading_rad));
    return displacement;
}

TurnResult turn::to_heading(const Kinematics& own, double target_heading_deg, double load_factor_g, double end_speed_kt)
{
    const double speed_kt = average_speed_kt(own.tas_kt, end_speed_kt);

    TurnResult result;
    result.turn_deg = angle_deg(own.heading_deg, target_heading_deg);
    result.end_heading_deg = wrap_180(target_heading_deg);
    result.radius_m = radius_m(speed_kt, load_factor_g);
    result.time_s = time_s(result.turn_deg, result.radius_m, speed_kt);
    result.displacement_nm = displacement_nm(own.heading_deg, result.turn_deg, result.radius_m);
    return result;
}
