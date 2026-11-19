#include "control/maneuvers/pursuit.hpp"

#include <cmath>

#include "common/angles.hpp"

void Pursuit::set(PursuitMode mode, double throttle)
{
    mode_ = mode;
    throttle_ = throttle;
}

Phase Pursuit::phase() const
{
    switch (mode_)
    {
    case PursuitMode::Pure: return Phase::PurePursuit;
    case PursuitMode::Lag:  return Phase::LagPursuit;
    case PursuitMode::Lead: return Phase::LeadPursuit;
    case PursuitMode::Collision: return Phase::Collision;
    }
    return Phase::PurePursuit;   // erişilmez; -Wswitch her mod için case ister
}

FlightCommand Pursuit::update(const Aircraft& own)
{
    FlightCommand command = own.command;
    command.set_heading_deg(target_heading_deg(own))
           .set_throttle(throttle_)
           .set_g_limit(G_LIMIT, own.profile.max_g);
    return command;
}

double Pursuit::target_heading_deg(const Aircraft& own) const
{
    const double bearing_deg = own.picture.bearing_deg;
    if (mode_ == PursuitMode::Collision)
    {
        double base_heading_deg = own.kinematics.heading_deg;
        if (own.picture.closure_kt <= 0.0)
        {
            base_heading_deg = bearing_deg;   // kapanma yoksa önce hedefe yönel
        }
        return wrap_180(base_heading_deg + KP_S * own.picture.los_rate_deg_s);   // LOS sıfırlanınca mevcut kesişme yönünü koru
    }

    double ahead_sign = 0.0;   // LOS dönmüyorsa lag ve lead de doğrudan hedefe yönelir
    if (own.picture.los_rate_deg_s > 0.0)
    {
        ahead_sign = 1.0;
    }
    else if (own.picture.los_rate_deg_s < 0.0)
    {
        ahead_sign = -1.0;
    }

    switch (mode_)
    {
    case PursuitMode::Pure:
        return wrap_180(bearing_deg);
    case PursuitMode::Lag:
        return wrap_180(bearing_deg - ahead_sign * LAG_DEG);
    case PursuitMode::Lead:
        return wrap_180(bearing_deg + ahead_sign * LEAD_DEG);
    case PursuitMode::Collision:
        break;   // yukarıda döndü
    }
    return wrap_180(bearing_deg);
}

ManeuverStatus Pursuit::status(const Aircraft& own) const
{
    if (mode_ == PursuitMode::Collision)
    {
        const bool closing = own.picture.range_2d_nm > 0.0 && own.picture.closure_kt > 0.0;
        if (closing && std::abs(own.picture.los_rate_deg_s) <= LOS_RATE_TOLERANCE_DEG_S)
        {
            return ManeuverStatus::Satisfied;
        }
        return ManeuverStatus::Running;
    }
    const double heading_error_deg = wrap_180(target_heading_deg(own) - own.kinematics.heading_deg);
    if (std::abs(heading_error_deg) <= ANGLE_TOLERANCE_DEG)
    {
        return ManeuverStatus::Satisfied;
    }
    return ManeuverStatus::Running;
}
