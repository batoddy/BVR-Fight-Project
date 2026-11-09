#include "control/maneuver.hpp"

#include <cmath>
#include <stdexcept>

#include "calculations/geometry.hpp"
#include "common/angles.hpp"
#include "model/missile.hpp"

// ========================================================================================================
// ORTAK
// ========================================================================================================

namespace
{
    constexpr double HEADING_LAG_S = 0.0;   // s, yön kontrolünün gecikmesi (flight_control.py TAU_HEAD)
    constexpr double LOS_RATE_KP = 1.3;     // birimsiz, gecikme payının çarpanı

    // Bearing kaydıkça uçak hedef yönün gerisinde kalır; düşmana göre açı tutan manevralar hedefi bu kadar ileri alır.
    double los_lead_deg(const Aircraft& own)
    {
        return LOS_RATE_KP * HEADING_LAG_S * own.picture.los_rate_deg_s;   // gecikme payı = KP × gecikme × LOS hızı
    }

    // Notch/Drag'in kaçtığı yön: bana gelen füze varsa füze, yoksa düşman uçak.
    double escape_bearing_deg(const Aircraft& own)
    {
        const Missile* threat = own.picture.threat;
        return threat ? geometry::bearing_deg(own.kinematics, threat->kinematics) : own.picture.bearing_deg;
    }

    double escape_ata_deg(const Aircraft& own)
    {
        const Missile* threat = own.picture.threat;
        return threat ? geometry::ata_deg(own.kinematics, threat->kinematics) : own.picture.ata_deg;
    }
}

void Maneuvers::fly(Aircraft& own, Maneuver& maneuver)
{
    if (active_ != &maneuver)   // yeni manevra: girişe bağlı hedefleri kilitle
    {
        maneuver.enter(own);
        active_ = &maneuver;
    }
    own.phase = maneuver.phase();
    own.command = maneuver.update(own);
}

// ========================================================================================================
// HOLD  —  girişteki yön ve irtifayı koru
// ========================================================================================================

void Hold::enter(const Aircraft& own)
{
    target_heading_deg_ = own.kinematics.heading_deg;
    target_altitude_ft_ = own.kinematics.altitude_ft;
    has_target_ = true;
}

FlightCommand Hold::update(const Aircraft& own)
{
    if (!has_target_)
    {
        enter(own);
    }
    FlightCommand command = own.command;
    command.set_heading_deg(target_heading_deg_)
           .set_altitude_ft(target_altitude_ft_)
           .set_throttle(THROTTLE)
           .set_g_limit(G_LIMIT, own.profile.max_g);
    return command;
}

ManeuverStatus Hold::status(const Aircraft& own) const
{
    const double heading_error_deg = wrap_180(target_heading_deg_ - own.kinematics.heading_deg);
    const double altitude_error_ft = target_altitude_ft_ - own.kinematics.altitude_ft;
    if (has_target_ && std::abs(heading_error_deg) <= ANGLE_TOLERANCE_DEG
        && std::abs(altitude_error_ft) <= ALTITUDE_TOLERANCE_FT)
    {
        return ManeuverStatus::Satisfied;
    }
    return ManeuverStatus::Running;
}

// ========================================================================================================
// OUT  —  düşmana kuyruk dön, alçal, tam art yakıcı; düşman arkada kalınca biter
// ========================================================================================================

void Out::enter(const Aircraft& own)
{
    const double entry_ft = own.kinematics.altitude_ft;
    const double descended_ft = entry_ft - DESCEND_FT;

    if (entry_ft <= FLOOR_FT)
    {
        target_altitude_ft_ = entry_ft;       // zaten tabanda ya da altında: irtifamı koru
    }
    else if (descended_ft < FLOOR_FT)
    {
        target_altitude_ft_ = FLOOR_FT;       // tam alçalırsam tabanın altına düşerim: tabanda dur
    }
    else
    {
        target_altitude_ft_ = descended_ft;   // DESCEND_FT alçal
    }
}

FlightCommand Out::update(const Aircraft& own)
{
    const double away_from_enemy_deg = own.picture.bearing_deg - 180.0 + los_lead_deg(own);   // düşman tam arkamda

    FlightCommand command = own.command;
    command.set_heading_deg(away_from_enemy_deg)
           .set_altitude_ft(target_altitude_ft_)
           .set_throttle(THROTTLE)
           .set_g_limit(own.profile.out_g, own.profile.max_g);   // NEZ/MAR hesabıyla aynı g
    return command;
}

ManeuverStatus Out::status(const Aircraft& own) const
{
    const double enemy_angle_off_nose_deg = std::abs(own.picture.ata_deg);
    if (enemy_angle_off_nose_deg > DONE_ATA_DEG)   // düşman arkamda kaldı
    {
        return ManeuverStatus::Done;
    }
    return ManeuverStatus::Running;
}

// ========================================================================================================
// COMMIT  —  düşmana doğru düz uç (saf takip)
// ========================================================================================================

FlightCommand Commit::update(const Aircraft& own)
{
    FlightCommand command = own.command;   // irtifa aynı
    command.set_heading_deg(own.picture.bearing_deg)
           .set_throttle(THROTTLE)
           .set_g_limit(G_LIMIT, own.profile.max_g);
    return command;
}

ManeuverStatus Commit::status(const Aircraft& own) const
{
    if (std::abs(own.picture.ata_deg) <= ANGLE_TOLERANCE_DEG)
    {
        return ManeuverStatus::Satisfied;
    }
    return ManeuverStatus::Running;
}

// ========================================================================================================
// RECOMMIT  —  düşmana dönüp atış açısına gel
// ========================================================================================================

namespace
{
    ManeuverSide recommit_side_for(const Aircraft& own)   // kısa taraf: düşman hangi tarafımdaysa o tarafa dön
    {
        if (own.picture.ata_deg >= 0.0)
        {
            return ManeuverSide::Right;
        }
        return ManeuverSide::Left;
    }
}

void Recommit::enter(const Aircraft& own)
{
    side_ = recommit_side_for(own);
    has_reached_ = false;
}

// ATA = bearing − heading. Right: düşman burnun sağında kalır (ATA +), Left: solunda (ATA −).
FlightCommand Recommit::update(const Aircraft& own)
{
    const double fire_ata_deg = own.profile.recommit_fire_ata_deg;

    if (status(own) == ManeuverStatus::Done)
    {
        has_reached_ = true;   // bir kez vardıktan sonra 20°'ye geri dönülmez
    }
    double target_ata_deg = fire_ata_deg - TURN_INSIDE_DEG;   // 30 − 10 = 20: dönüş 30°'ye kadar sert kalsın
    if (has_reached_)
    {
        target_ata_deg = fire_ata_deg;                         // 30°'de dur
    }

    const double lead_deg = los_lead_deg(own);

    FlightCommand command = own.command;   // irtifa aynı
    command.set_throttle(THROTTLE)
           .set_g_limit(own.profile.out_g, own.profile.max_g);   // sert dönüş: out ile aynı g
    if (side_ == ManeuverSide::Right)
    {
        command.set_heading_deg(own.picture.bearing_deg - target_ata_deg + lead_deg);
    }
    else
    {
        command.set_heading_deg(own.picture.bearing_deg + target_ata_deg + lead_deg);
    }
    return command;
}

ManeuverStatus Recommit::status(const Aircraft& own) const
{
    const double enemy_angle_off_nose_deg = std::abs(own.picture.ata_deg);
    if (enemy_angle_off_nose_deg <= own.profile.recommit_fire_ata_deg + DONE_TOLERANCE_DEG)   // atış açısına indi
    {
        return ManeuverStatus::Done;
    }
    return ManeuverStatus::Running;
}

// ========================================================================================================
// CRANK  —  düşmanı radar gimbal sınırının biraz içinde tutarak yanal kaçış
// ========================================================================================================

namespace
{
    ManeuverSide crank_side_for(const Aircraft& own)
    {
        if (own.picture.ta_deg >= 0.0)
        {
            return ManeuverSide::Left;    // düşmanın burnunun sağındayım: sola dönerim
        }
        return ManeuverSide::Right;       // düşmanın burnunun solundayım: sağa dönerim
    }
}

void Crank::enter(const Aircraft& own)
{
    side_ = crank_side_for(own);
}

// ATA = bearing − heading. Right: düşman burnun solunda kalır (ATA −), Left: sağında (ATA +).
FlightCommand Crank::update(const Aircraft& own)
{
    const double limit_deg = own.profile.radar_gimbal_deg - OFFSET_DEG;
    const double lead_deg = los_lead_deg(own);

    FlightCommand command = own.command;   // irtifa aynı
    command.set_throttle(THROTTLE)
           .set_g_limit(G_LIMIT, own.profile.max_g);
    if (side_ == ManeuverSide::Right)
    {
        command.set_heading_deg(own.picture.bearing_deg + limit_deg + lead_deg);
    }
    else
    {
        command.set_heading_deg(own.picture.bearing_deg - limit_deg + lead_deg);
    }
    return command;
}

bool Crank::in_radar_sight(const Aircraft& own) const
{
    return std::abs(own.picture.ata_deg) < own.profile.radar_gimbal_deg;
}

ManeuverStatus Crank::status(const Aircraft& own) const
{
    double target_ata_deg = own.profile.radar_gimbal_deg - OFFSET_DEG;
    if (side_ == ManeuverSide::Right)
    {
        target_ata_deg = -target_ata_deg;
    }
    const double angle_error_deg = wrap_180(target_ata_deg - own.picture.ata_deg);
    if (in_radar_sight(own) && std::abs(angle_error_deg) <= ANGLE_TOLERANCE_DEG)
    {
        return ManeuverStatus::Satisfied;
    }
    return ManeuverStatus::Running;
}

// ========================================================================================================
// BEAM  —  düşman uçağa dik uç (3/9 hattı)
// ========================================================================================================

void Beam::enter(const Aircraft& own)
{
    side_ = ManeuverSide::Right;      // düşman solumda: sağa dönerim
    if (own.picture.ata_deg > 0.0)
    {
        side_ = ManeuverSide::Left;   // düşman sağımda: sola dönerim
    }
}

FlightCommand Beam::update(const Aircraft& own)
{
    const double lead_deg = los_lead_deg(own);

    double beam_heading_deg = own.picture.bearing_deg + 90.0 + lead_deg;   // Right: düşman 9 hizamda (ATA −90)
    if (side_ == ManeuverSide::Left)
    {
        beam_heading_deg = own.picture.bearing_deg - 90.0 + lead_deg;      // Left: düşman 3 hizamda (ATA +90)
    }

    FlightCommand command = own.command;   // irtifa aynı
    command.set_heading_deg(beam_heading_deg)
           .set_throttle(THROTTLE)
           .set_g_limit(G_LIMIT, own.profile.max_g);
    return command;
}

ManeuverStatus Beam::status(const Aircraft& own) const
{
    double target_ata_deg = -90.0;
    if (side_ == ManeuverSide::Left)
    {
        target_ata_deg = 90.0;
    }
    if (std::abs(wrap_180(target_ata_deg - own.picture.ata_deg)) <= ANGLE_TOLERANCE_DEG)
    {
        return ManeuverStatus::Satisfied;
    }
    return ManeuverStatus::Running;
}

// ========================================================================================================
// NOTCH  —  füzeye dik uç
// ========================================================================================================

FlightCommand Notch::update(const Aircraft& own)
{
    const double bearing_deg = escape_bearing_deg(own);
    double notch_heading_deg = bearing_deg + 90.0;   // tehdit solumda kalsın
    if (escape_ata_deg(own) > 0.0)
    {
        notch_heading_deg = bearing_deg - 90.0;      // tehdit sağımda kalsın
    }

    FlightCommand command = own.command;   // irtifa aynı
    command.set_heading_deg(notch_heading_deg)
           .set_throttle(THROTTLE)
           .set_g_limit(G_LIMIT, own.profile.max_g);
    return command;
}

ManeuverStatus Notch::status(const Aircraft& own) const
{
    if (std::abs(std::abs(escape_ata_deg(own)) - 90.0) <= ANGLE_TOLERANCE_DEG)   // tehdit 3 ya da 9 hizamda
    {
        return ManeuverStatus::Satisfied;
    }
    return ManeuverStatus::Running;
}

// ========================================================================================================
// DRAG  —  füzeye sırt dön
// ========================================================================================================

FlightCommand Drag::update(const Aircraft& own)
{
    FlightCommand command = own.command;   // irtifa aynı
    command.set_heading_deg(escape_bearing_deg(own) - 180.0)   // tehdit tam arkamda
           .set_throttle(THROTTLE)
           .set_g_limit(own.profile.out_g, own.profile.max_g);
    return command;
}

ManeuverStatus Drag::status(const Aircraft& own) const
{
    if (std::abs(escape_ata_deg(own)) >= 180.0 - ANGLE_TOLERANCE_DEG)   // tehdit 6 hizamda
    {
        return ManeuverStatus::Satisfied;
    }
    return ManeuverStatus::Running;
}

// ========================================================================================================
// OFFSET  —  düşman yönünden verilen açı kadar yana uç
// ========================================================================================================

void Offset::set(double offset_deg, ManeuverSide side)
{
    if (!std::isfinite(offset_deg) || offset_deg < 0.0 || offset_deg > 180.0)
    {
        throw std::invalid_argument("Offset acisi sonlu ve 0..180 derece olmalidir");
    }
    offset_deg_ = offset_deg;
    side_ = side;
}

FlightCommand Offset::update(const Aircraft& own)
{
    double target_heading_deg = own.picture.bearing_deg + offset_deg_;
    if (side_ == ManeuverSide::Left)
    {
        target_heading_deg = own.picture.bearing_deg - offset_deg_;
    }
    FlightCommand command = own.command;
    command.set_heading_deg(target_heading_deg + los_lead_deg(own))
           .set_throttle(THROTTLE)
           .set_g_limit(G_LIMIT, own.profile.max_g);

    return command;
}

ManeuverStatus Offset::status(const Aircraft& own) const
{
    double target_ata_deg = -offset_deg_;
    if (side_ == ManeuverSide::Left)
    {
        target_ata_deg = offset_deg_;
    }
    if (std::abs(wrap_180(target_ata_deg - own.picture.ata_deg)) <= ANGLE_TOLERANCE_DEG)
    {
        return ManeuverStatus::Satisfied;
    }
    return ManeuverStatus::Running;
}

// ========================================================================================================
// FLANK  —  düşmanın burnuna göre verilen yan açıyı koru
// ========================================================================================================

void Flank::set(double target_ta_deg, double tolerance_deg)
{
    if (!std::isfinite(target_ta_deg) || !std::isfinite(tolerance_deg)
        || tolerance_deg < 0.0 || tolerance_deg > 180.0)
    {
        throw std::invalid_argument("Flank acisi sonlu, tolerans 0..180 derece olmalidir");
    }
    target_ta_deg_ = wrap_180(target_ta_deg);
    tolerance_deg_ = tolerance_deg;
}

void Flank::enter(const Aircraft& own)
{
    range_nm_ = own.picture.range_2d_nm;
}

FlightCommand Flank::update(const Aircraft& own)
{
    FlightCommand command = own.command;
    command.set_heading_deg(geometry::flank_heading_deg(own.kinematics, own.picture, target_ta_deg_, range_nm_, RESPONSE_S))
           .set_throttle(THROTTLE)
           .set_g_limit(G_LIMIT, own.profile.max_g);

    return command;
}

ManeuverStatus Flank::status(const Aircraft& own) const
{
    const double angle_error_deg = wrap_180(target_ta_deg_ - own.picture.ta_deg);
    if (std::abs(angle_error_deg) <= tolerance_deg_)
    {
        return ManeuverStatus::Satisfied;
    }
    return ManeuverStatus::Running;
}

// ========================================================================================================
// COLLISION  —  LOS hızını sıfırla: çarpışma rotası
// ========================================================================================================

FlightCommand Collision::update(const Aircraft& own)
{
    double base_heading_deg = own.kinematics.heading_deg;
    if (own.picture.closure_kt <= 0.0)
    {
        base_heading_deg = own.picture.bearing_deg;   // kapanma yoksa önce hedefe yönel
    }
    FlightCommand command = own.command;
    const double correction_deg = KP_S * own.picture.los_rate_deg_s;
    command.set_heading_deg(base_heading_deg + correction_deg)   // LOS sıfırlanınca mevcut kesişme yönünü koru
           .set_throttle(THROTTLE)
           .set_g_limit(G_LIMIT, own.profile.max_g);

    return command;
}

ManeuverStatus Collision::status(const Aircraft& own) const
{
    if (own.picture.range_2d_nm > 0.0 && own.picture.closure_kt > 0.0
        && std::abs(own.picture.los_rate_deg_s) <= LOS_RATE_TOLERANCE_DEG_S)
    {
        return ManeuverStatus::Satisfied;
    }
    return ManeuverStatus::Running;
}

// ========================================================================================================
// FLY HEADING  —  verilen sabit yöne uç
// ========================================================================================================

void FlyHeading::set(double heading_deg)
{
    if (!std::isfinite(heading_deg))
    {
        throw std::invalid_argument("FlyHeading yonu sonlu olmalidir");
    }
    heading_deg_ = wrap_180(heading_deg);
}

FlightCommand FlyHeading::update(const Aircraft& own)
{
    FlightCommand command = own.command;
    command.set_heading_deg(heading_deg_)
           .set_throttle(THROTTLE)
           .set_g_limit(G_LIMIT, own.profile.max_g);

    return command;
}

ManeuverStatus FlyHeading::status(const Aircraft& own) const
{
    if (std::abs(wrap_180(heading_deg_ - own.kinematics.heading_deg)) <= ANGLE_TOLERANCE_DEG)
    {
        return ManeuverStatus::Satisfied;
    }
    return ManeuverStatus::Running;
}

// ========================================================================================================
// SNAKE  —  periyodik sağ-sol (İSKELET)
// ========================================================================================================

void Snake::enter(const Aircraft& own)
{
    base_heading_deg_ = own.kinematics.heading_deg;
    // start_s_ = now_s;   // World::sim_time_s otopilota iletilince
}

FlightCommand Snake::update(const Aircraft& own)
{
    FlightCommand command = own.command;
    command.set_throttle(THROTTLE);

    // const double elapsed_s = now_s - start_s_;
    // const bool right_half = static_cast<int>(elapsed_s / HALF_PERIOD_S) % 2 == 0;
    // if (right_half) { command.set_heading_deg(base_heading_deg_ + AMPLITUDE_DEG); }
    // else            { command.set_heading_deg(base_heading_deg_ - AMPLITUDE_DEG); }

    return command;
}

// ========================================================================================================
// PURSUIT  —  pure, lag, lead takip
// ========================================================================================================

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
    }
    return wrap_180(bearing_deg);
}

ManeuverStatus Pursuit::status(const Aircraft& own) const
{
    const double heading_error_deg = wrap_180(target_heading_deg(own) - own.kinematics.heading_deg);
    if (std::abs(heading_error_deg) <= ANGLE_TOLERANCE_DEG)
    {
        return ManeuverStatus::Satisfied;
    }
    return ManeuverStatus::Running;
}
