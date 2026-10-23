#include "calculations/missile_geometry.hpp"
#include "calculations/geometry.hpp"

double MissileGeometry::range_nm(const Missile& missile, const Aircraft& target)
{
    return Geometry::range_3d_nm(missile.kinematics, target.kinematics);
}

double MissileGeometry::closure_kt(const Missile& missile, const Aircraft& target)
{
    return Geometry::rate_of_closure_kt(missile.kinematics, target.kinematics);
}

double MissileGeometry::time_to_go_s(double range_nm, double closure_kt)
{
    if (closure_kt <= 0.0)
    {
        return -1.0;   // yaklaşmıyor: hedefe hiç varmaz
    }
    // NM / (NM/saat) saat verir; dışarıya saniye döndür.
    return SECONDS_PER_HOUR * range_nm / closure_kt;
}

bool MissileGeometry::pitbull(const Missile& missile, double range_nm)
{
    double pitbull_range_nm = missile.profile.pitbull_range_nm;
    return pitbull_range_nm > 0.0 && range_nm < pitbull_range_nm;
}
Ned MissileGeometry::los_nm(const Missile& missile, const Aircraft& target)
{
    return Geometry::los_nm(missile.kinematics,target.kinematics);
}
