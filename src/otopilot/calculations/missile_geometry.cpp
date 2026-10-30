#include "calculations/missile_geometry.hpp"

#include "calculations/geometry.hpp"
#include "common/units.hpp"

double missile_geometry::range_nm(const Missile& missile, const Aircraft& target)
{
    return geometry::range_3d_nm(missile.kinematics, target.kinematics);
}

double missile_geometry::closure_kt(const Missile& missile, const Aircraft& target)
{
    return geometry::closure_kt(missile.kinematics, target.kinematics);
}

std::optional<double> missile_geometry::time_to_go_s(double range_nm, double closure_kt)
{
    if (closure_kt <= 0.0)
    {
        return std::nullopt;   // yaklaşmıyor: hedefe hiç varmaz
    }
    return travel_time_s(range_nm, closure_kt);
}

bool missile_geometry::is_pitbull(const Missile& missile, double range_nm)
{
    const double pitbull_range_nm = missile.profile.pitbull_range_nm;
    const bool has_own_seeker = pitbull_range_nm > 0.0;   // yalnız Fox3; Fox1/Fox2'de 0
    const bool close_enough = range_nm < pitbull_range_nm;
    return has_own_seeker && close_enough;
}
