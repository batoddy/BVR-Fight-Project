#include "geometry.hpp"
#include <cmath>
#include "angles.hpp"

double Geometry::range_m(const Kinematics& own, const Kinematics& target)
{
    // TODO: hesap. Şimdilik 0; bağlantılar doğru mu diye bakıyoruz.
    (void)own;
    (void)target;
    return 0.0;
}

double Geometry::bearing_deg_180(const Kinematics& own, const Kinematics& target)
{
    // TODO: hesap. Sonucu wrap_180 ile ±180'e getirmeyi unutma.
    (void)own;
    (void)target;
    return 0.0;
}
