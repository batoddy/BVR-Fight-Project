#include "control/maneuver.hpp"

#include <cmath>

#include "common/angles.hpp"
#include "calculations/geometry.hpp"

// ========================================================================================================
// ORTAK
// ========================================================================================================

namespace
{
    constexpr double HEADING_LAG_S = 2.0;   // s, yön kontrolünün gecikmesi (flight_control.py TAU_HEAD)
    constexpr double LOS_RATE_KP = 1.3;     // birimsiz, gecikme payının çarpanı

    // Bearing kaydıkça uçak hedef yönün gerisinde kalır; düşmana göre açı tutan manevralar hedefi bu kadar ileri alır.
    double los_lead_deg(const Aircraft& own)
    {
        return LOS_RATE_KP * HEADING_LAG_S * own.picture.los_rate_deg_s;   // gecikme payı = KP × gecikme × LOS hızı
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
// HOLD  —  şu anki yön ve irtifayla, seyir gazıyla devam
// ========================================================================================================

FlightCommand Hold::update(const Aircraft& own)
{
    FlightCommand command = own.command;
    command.set_heading_deg(own.kinematics.heading_deg)
           .set_altitude_ft(own.kinematics.altitude_ft)
           .set_throttle(THROTTLE);
    return command;
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

    double out_throttle = THROTTLE;
    if (status(own) == ManeuverStatus::Done)
    {
        out_throttle = EXIT_THROTTLE;   // düşman arkada: art yakıcıyı bırak
    }

    FlightCommand command = own.command;
    command.set_heading_deg(away_from_enemy_deg)
           .set_altitude_ft(target_altitude_ft_)
           .set_throttle(out_throttle)
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

// ========================================================================================================
// NOTCH  —  füzeye dik uç (İSKELET)
// ========================================================================================================

FlightCommand Notch::update(const Aircraft& own)
{
    FlightCommand command = own.command;
    command.set_throttle(THROTTLE);

    // Gövde yazılınca: #include "calculations/geometry.hpp"
    // const Missile* threat = own.picture.threat;
    // if (threat == nullptr) { return command; }                                                     // füze yok
    // const double threat_bearing_deg = geometry::bearing_deg(own.kinematics, threat->kinematics);
    // const double threat_ata_deg = geometry::ata_deg(own.kinematics, threat->kinematics);
    // double notch_heading_deg = threat_bearing_deg + 90.0;                                           // füze solumda
    // if (threat_ata_deg > 0.0) { notch_heading_deg = threat_bearing_deg - 90.0; }                    // füze sağımda
    // command.set_heading_deg(notch_heading_deg);

    return command;
}

// ========================================================================================================
// DRAG  —  füzeye sırt dön (İSKELET)
// ========================================================================================================

FlightCommand Drag::update(const Aircraft& own)
{
    FlightCommand command = own.command;
    command.set_throttle(THROTTLE);

    const Missile* threat = own.picture.threat;
    if (threat == nullptr) 
    {
        return command;
    }

    command.set_heading_deg(geometry::bearing_deg(own.kinematics, threat->kinematics) - 180.0);   // füze tam arkamda

    return command;
}

ManeuverStatus  Drag::status(const Aircraft& own)
{
    ManeuverStatus  success = false;
    if(std::fabs(geometry::ata_deg(own.kinematics, own.picture.threat->kinematics)) > 180 - DRAG_OFFSET_DEG)
    {
        success = true;
    }
    return success;
}

// ========================================================================================================
// OFFSET  —  düşman yönünden verilen açı kadar yana uç (İSKELET)
// ========================================================================================================

void Offset::set(double offset_deg, ManeuverSide side)
{
    offset_deg_ = offset_deg;
    side_ = side;
}

FlightCommand Offset::update(const Aircraft& own)
{
    FlightCommand command = own.command;
    command.set_throttle(THROTTLE);

    // if (side_ == ManeuverSide::Right) { command.set_heading_deg(own.picture.bearing_deg + offset_deg_); }
    // else                              { command.set_heading_deg(own.picture.bearing_deg - offset_deg_); }

    return command;
}

// ========================================================================================================
// FLANK  —  düşmanın burnuna göre yan açıda kal (İSKELET)
// ========================================================================================================

// |ta| büyümeli: düşmanın etrafında burnundan uzaklaşan yöne dolanırım. Düşman kuzeye uçarken ben onun 30° sağındaysam
// (ta = +30) saat yönünde ilerlemeliyim; bu yön benden düşmana bearing'in 90° solu. Yani ta ≥ 0 iken offset ÇIKARILIR.
// Düşmanın kendi hareketi hesaba katılmadı; gövde yazılırken simülasyonda doğrulanacak.
FlightCommand Flank::update(const Aircraft& own)
{
    FlightCommand command = own.command;
    command.set_throttle(THROTTLE);

    // const double offset_deg = 40.0;                                                                          // ya da ta hatasıyla orantılı
    // if (own.picture.ta_deg >= 0.0) { command.set_heading_deg(own.picture.bearing_deg - offset_deg); }       // düşman sağımda kalır
    // else                           { command.set_heading_deg(own.picture.bearing_deg + offset_deg); }       // düşman solumda kalır

    return command;
}

ManeuverStatus Flank::status(const Aircraft& own) const
{
    (void)own;

    // if (std::abs(own.picture.ta_deg) >= TARGET_TA_DEG) { return ManeuverStatus::Done; }

    return ManeuverStatus::Running;
}

// ========================================================================================================
// COLLISION  —  LOS hızını sıfırla: çarpışma rotası (İSKELET)
// ========================================================================================================

FlightCommand Collision::update(const Aircraft& own)
{
    FlightCommand command = own.command;
    command.set_throttle(THROTTLE);

    // const double kp_s = 10.0;                                                                   // s, LOS hızını açıya çeviren kazanç; denenecek
    // command.set_heading_deg(own.picture.bearing_deg + kp_s * own.picture.los_rate_deg_s);    // bearing kayan tarafa dön

    return command;
}

// ========================================================================================================
// FLY HEADING  —  verilen sabit yöne uç (İSKELET)
// ========================================================================================================

void FlyHeading::set(double heading_deg)
{
    heading_deg_ = heading_deg;
}

FlightCommand FlyHeading::update(const Aircraft& own)
{
    FlightCommand command = own.command;
    command.set_throttle(THROTTLE);

    // command.set_heading_deg(heading_deg_);

    return command;
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
// PURSUIT  —  pure, lag, lead takip (İSKELET)
// ========================================================================================================

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
    command.set_throttle(THROTTLE);

    // const double bearing_deg = own.picture.bearing_deg;
    // const double ahead_sign = (own.picture.los_rate_deg_s >= 0.0) ? 1.0 : -1.0;   // LOS + ise düşmanın önü bearing'in + tarafında
    // switch (mode_)
    // {
    // case PursuitMode::Pure: command.set_heading_deg(bearing_deg); break;
    // case PursuitMode::Lag:  command.set_heading_deg(bearing_deg - ahead_sign * LAG_DEG); break;
    // case PursuitMode::Lead: command.set_heading_deg(bearing_deg + ahead_sign * 20.0); break;   // öncü açı: sin = V_düşman × sin(TA) / V_ben
    // }

    return command;
}
