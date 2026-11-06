#include "control/maneuver.hpp"

#include <cmath>

#include "calculations/geometry.hpp"
#include "common/angles.hpp"

// ========================================================================================================
// ORTAK
//
//   los_lead_deg(own)               yön kontrolünün gecikmesine karşı hedef yönün ne kadar ileri alınacağı
//   Maneuvers::fly(own, maneuver)   değişen manevraya giriş yapar, ardından fazı ve komutu yazar
// ========================================================================================================

namespace
{
    constexpr double HEADING_LAG_S = 2.0;   // s, yön kontrolünün gecikmesi (flight_control.py TAU_HEAD)
    constexpr double LOS_RATE_KP = 1.3;     // birimsiz, gecikme payının çarpanı

    // Düşmana göre açı tutan manevralar (Crank, Recommit) için: bearing kaydıkça uçak geride kalır.
    //   gecikme payı = LOS_RATE_KP × HEADING_LAG_S × LOS hızı
    double los_lead_deg(const Aircraft& own)
    {
        return LOS_RATE_KP * HEADING_LAG_S * own.picture.los_rate_deg_s;
    }
}

void Maneuvers::fly(Aircraft& own, Maneuver& maneuver)
{
    if (active_ != &maneuver)
    {
        maneuver.enter(own);
        active_ = &maneuver;
    }
    own.phase = maneuver.phase();
    own.command = maneuver.update(own);
}

// ========================================================================================================
// HOLD  —  şu anki yön ve irtifayla, aynı gazla devam
//
//   update(own)   yön ve irtifa = şu anki ölçüm; g sınırı kaldırılır
// ========================================================================================================

FlightCommand Hold::update(const Aircraft& own)
{
    FlightCommand command = own.command;   // gaz aynı kalır
    command.set_heading_deg(own.kinematics.heading_deg)
           .set_altitude_ft(own.kinematics.altitude_ft)
           .clear_g_limit();
    return command;
}

// ========================================================================================================
// OUT  —  düşmana kuyruk dön, alçal, tam art yakıcı; düşman arkada kalınca biter
//
//   enter(own)    girişte bir kez: hedef irtifayı kilitler (DESCEND_FT alçal, FLOOR_FT altına inme)
//   update(own)   her step: bearing − 180'e dön, hedef irtifaya in, tam art yakıcı, out g'si
//   status(own)   düşman arkamda kaldıysa (|ATA| > DONE_ATA_DEG) Done
// ========================================================================================================

// Hedef irtifa, girişte bir kez:
//   Zaten FLOOR_FT'de ya da altındaysam:           irtifamı koru.
//   DESCEND_FT inince FLOOR_FT'nin altına düşersem: FLOOR_FT'de dur.
//   Diğer durumda:                                  DESCEND_FT alçal.
void Out::enter(const Aircraft& own)
{
    const double entry_ft = own.kinematics.altitude_ft;
    const double descended_ft = entry_ft - DESCEND_FT;

    if (entry_ft <= FLOOR_FT)
    {
        target_altitude_ft_ = entry_ft;
    }
    else if (descended_ft < FLOOR_FT)
    {
        target_altitude_ft_ = FLOOR_FT;
    }
    else
    {
        target_altitude_ft_ = descended_ft;
    }
}

// Düşmanı tam arkama alacak yöne dön (bearing − 180), alçal, tam art yakıcı, out g'siyle.
FlightCommand Out::update(const Aircraft& own)
{
    const double away_from_enemy_deg = own.picture.bearing_deg - 180.0;

    FlightCommand command = own.command;
    command.set_heading_deg(away_from_enemy_deg)
           .set_altitude_ft(target_altitude_ft_)
           .set_throttle(THROTTLE)
           .set_g_limit(own.profile.out_g, own.profile.max_g);   // NEZ/MAR ile aynı g
    return command;
}

// Bitti mi: düşman arkamda kaldı. Burnuma göre açısı (sağdan ya da soldan) DONE_ATA_DEG'den büyük.
ManeuverStatus Out::status(const Aircraft& own) const
{
    const double enemy_angle_off_nose_deg = std::abs(own.picture.ata_deg);
    if (enemy_angle_off_nose_deg > DONE_ATA_DEG)
    {
        return ManeuverStatus::Done;
    }
    return ManeuverStatus::Running;
}

// ========================================================================================================
// COMMIT  —  düşmana doğru düz uç (saf takip)
//
//   update(own)   yön = düşmanın bearing'i; irtifa aynı, gaz THROTTLE
// ========================================================================================================

FlightCommand Commit::update(const Aircraft& own)
{
    FlightCommand command = own.command;   // irtifa aynı
    command.set_heading_deg(own.picture.bearing_deg)
           .set_throttle(THROTTLE);
    return command;
}

// ========================================================================================================
// RECOMMIT  —  düşmana dönüp atış açısına gel
//
//   recommit_side_for(own)     kısa dönüş tarafı: düşman sağımdaysa (ATA ≥ 0) sağa, solumdaysa sola
//   enter(own)                 girişte bir kez: tarafı seçer (recommit_side_for)
//   update(own)                yön = bearing ∓ hedef ATA (Right −, Left +) + gecikme payı; irtifa aynı, gaz THROTTLE
//   status(own)                atış açısına geldiyse (|ATA| ≤ recommit_fire_ata_deg + DONE_TOLERANCE_DEG) Done
// ========================================================================================================

namespace
{
    // Dönüş, hesaplardaki gibi kısa taraftan: düşman hangi tarafımdaysa o tarafa dönerim.
    ManeuverSide recommit_side_for(const Aircraft& own)
    {
        if (own.picture.ata_deg >= 0.0)
        {
            return ManeuverSide::Right;
        }
        return ManeuverSide::Left;
    }
}

// Taraf girişte bir kez seçilir.
void Recommit::enter(const Aircraft& own)
{
    side_ = recommit_side_for(own);
}

// Sağa dönüşte düşman burnun sağında kalır (ATA +), sola dönüşte solunda (ATA −). ATA = bearing − heading.
// Bearing kaydıkça uçak geride kalır; hedef, bu gecikme kadar ileri alınır (los_lead_deg).
FlightCommand Recommit::update(const Aircraft& own)
{
    const double fire_ata_deg = own.profile.recommit_fire_ata_deg;

    // Atış açısına gelene kadar 20°'yi hedefle: dönüş 30°'ye kadar sert kalsın. Gelince 30°'de dur.
    double target_ata_deg = fire_ata_deg - TURN_INSIDE_DEG;   // 30 − 10 = 20
    if (status(own) == ManeuverStatus::Done)
    {
        target_ata_deg = fire_ata_deg;
    }

    const double lead_deg = los_lead_deg(own);

    FlightCommand command = own.command;   // irtifa aynı
    command.set_throttle(THROTTLE);
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


// Bitti mi: düşmanın burnuma göre açısı (sağdan ya da soldan) atış açısına indi.
ManeuverStatus Recommit::status(const Aircraft& own) const
{
    const double enemy_angle_off_nose_deg = std::abs(own.picture.ata_deg);
    if (enemy_angle_off_nose_deg <= own.profile.recommit_fire_ata_deg + DONE_TOLERANCE_DEG)
    {
        return ManeuverStatus::Done;
    }
    return ManeuverStatus::Running;
}

// ========================================================================================================
// CRANK  —  düşmanı radar gimbal sınırının biraz içinde tutarak yanal kaçış
//
//   crank_side_for(own)        düşmanın burnunun sağındaysam (TA ≥ 0) sola, solundaysam sağa crank
//   enter(own)                 girişte bir kez: tarafı seçer (crank_side_for)
//   update(own)                yön = bearing ± (gimbal − OFFSET_DEG) (Right +, Left −) + gecikme payı; irtifa aynı, gaz THROTTLE
//   in_radar_sight(own)        düşman radarın görüş konisinde mi (|ATA| < gimbal)
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

// Taraf girişte bir kez seçilir.
void Crank::enter(const Aircraft& own)
{
    side_ = crank_side_for(own);
}

// Sağa crank'te düşman burnun solunda kalır (ATA −), sola crank'te sağında (ATA +). ATA = bearing − heading.
// Bearing kaydıkça uçak geride kalır; hedef, bu gecikme kadar ileri alınır (los_lead_deg).
FlightCommand Crank::update(const Aircraft& own)
{
    const double limit_deg = own.profile.radar_gimbal_deg - OFFSET_DEG;
    const double lead_deg = los_lead_deg(own);

    FlightCommand command = own.command;   // irtifa aynı
    command.set_throttle(THROTTLE);
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

// Radarın görüş konisinde mi: düşmanın burnuma göre açısı (sağdan ya da soldan) gimbal sınırından küçük.
bool Crank::in_radar_sight(const Aircraft& own) const
{
    return (std::fabs(own.picture.ata_deg) < own.profile.radar_gimbal_deg);
}

// ========================================================================================================
// BEAM  —  düşman uçağa dik uç (3/9 hattı)
//
//   update(own)   yön = bearing ∓ 90 (taraf burnuma göre); irtifa aynı, gaz THROTTLE
// ========================================================================================================

// Düşmana dik uç: düşman 3 ya da 9 hizamda kalsın.
//   Düşman sağımdaysa (ATA > 0) sola dönerim: yön = bearing − 90, düşman 3 hizamda.
//   Düşman solumdaysa sağa dönerim:          yön = bearing + 90, düşman 9 hizamda.
// Taraf burnuma göre seçildiği için dönüş hep 90°'den kısadır.
FlightCommand Beam::update(const Aircraft& own)
{
    const double bearing_deg = own.picture.bearing_deg;

    double beam_heading_deg = bearing_deg + 90.0;   // düşman solumda
    if (own.picture.ata_deg > 0.0)
    {
        beam_heading_deg = bearing_deg - 90.0;      // düşman sağımda
    }

    FlightCommand command = own.command;   // irtifa aynı
    command.set_heading_deg(beam_heading_deg)
           .set_throttle(THROTTLE);
    return command;
}

// ========================================================================================================
// NOTCH  —  füzeye dik uç (İSKELET)
//
//   update(own)   örnek: yön = füzenin bearing'i ∓ 90 (taraf füzenin burnuma göre yerinden)
// ========================================================================================================

FlightCommand Notch::update(const Aircraft& own)
{
    FlightCommand command = own.command;

    // const Missile* threat = own.picture.threat;
    // if (threat == nullptr) { return command; }   // füze yok: komut değişmez
    //
    // const double threat_bearing_deg = geometry::bearing_deg(own.kinematics, threat->kinematics);
    // const double threat_ata_deg = geometry::ata_deg(own.kinematics, threat->kinematics);
    // double notch_heading_deg = threat_bearing_deg + 90.0;   // füze solumda
    // if (threat_ata_deg > 0.0)
    // {
    //     notch_heading_deg = threat_bearing_deg - 90.0;      // füze sağımda
    // }
    // command.set_heading_deg(notch_heading_deg);

    return command;
}

// ========================================================================================================
// DRAG  —  füzeye sırt dön (İSKELET)
//
//   update(own)   örnek: yön = füzenin bearing'i − 180
// ========================================================================================================

FlightCommand Drag::update(const Aircraft& own)
{
    FlightCommand command = own.command;

    // const Missile* threat = own.picture.threat;
    // if (threat == nullptr) { return command; }
    // command.set_heading_deg(geometry::bearing_deg(own.kinematics, threat->kinematics) - 180.0)
    //        .set_throttle(throttle::MRT);   // P-1290: drag MRT ile

    return command;
}

// ========================================================================================================
// OFFSET  —  düşman yönünden verilen açı kadar yana uç (İSKELET)
//
//   set(offset_deg, side)   açıyı ve tarafı ayarlar
//   update(own)             örnek: yön = bearing ± offset (Right +, Left −)
// ========================================================================================================

void Offset::set(double offset_deg, ManeuverSide side)
{
    offset_deg_ = offset_deg;
    side_ = side;
}

FlightCommand Offset::update(const Aircraft& own)
{
    FlightCommand command = own.command;

    // if (side_ == ManeuverSide::Right)
    // {
    //     command.set_heading_deg(own.picture.bearing_deg + offset_deg_);
    // }
    // else
    // {
    //     command.set_heading_deg(own.picture.bearing_deg - offset_deg_);
    // }

    return command;
}

// ========================================================================================================
// FLANK  —  düşmanın burnuna göre yan açıda kal (İSKELET)
//
//   update(own)   örnek: düşmanın burnundan uzaklaşan tarafa offset
//   status(own)   örnek: |ta| Flank aralığına girince Done
// ========================================================================================================

FlightCommand Flank::update(const Aircraft& own)
{
    FlightCommand command = own.command;

    // Düşmanın burnunun sağındaysam (ta ≥ 0) daha sağa açılmak için düşman solumda kalacak şekilde uçarım.
    // const double offset_deg = 40.0;   // ya da ta hatasıyla orantılı
    // if (own.picture.ta_deg >= 0.0)
    // {
    //     command.set_heading_deg(own.picture.bearing_deg + offset_deg);
    // }
    // else
    // {
    //     command.set_heading_deg(own.picture.bearing_deg - offset_deg);
    // }

    return command;
}

ManeuverStatus Flank::status(const Aircraft& own) const
{
    (void)own;

    // if (std::abs(own.picture.ta_deg) >= TARGET_TA_DEG)
    // {
    //     return ManeuverStatus::Done;
    // }

    return ManeuverStatus::Running;
}

// ========================================================================================================
// COLLISION  —  LOS hızını sıfırla: çarpışma rotası (İSKELET)
//
//   update(own)   örnek: yön = bearing + KP × LOS hızı
// ========================================================================================================

FlightCommand Collision::update(const Aircraft& own)
{
    FlightCommand command = own.command;

    // Bearing saat yönünde kayıyorsa (+) düşman önüme geçiyor: ben de o tarafa dönerim.
    // const double kp_s = 10.0;   // s, LOS hızını açıya çeviren kazanç; denenecek
    // command.set_heading_deg(own.picture.bearing_deg + kp_s * own.picture.los_rate_deg_s);

    return command;
}

// ========================================================================================================
// FLY HEADING  —  verilen sabit yöne uç (İSKELET)
//
//   set(heading_deg)   yönü ayarlar
//   update(own)        örnek: yön = heading_deg_
// ========================================================================================================

void FlyHeading::set(double heading_deg)
{
    heading_deg_ = heading_deg;
}

FlightCommand FlyHeading::update(const Aircraft& own)
{
    FlightCommand command = own.command;

    // command.set_heading_deg(heading_deg_);

    return command;
}

// ========================================================================================================
// SNAKE  —  periyodik sağ-sol (İSKELET)
//
//   enter(own)    örnek: girişteki yönü kilitler
//   update(own)   örnek: her HALF_PERIOD_S'de taraf değiştir
// ========================================================================================================

void Snake::enter(const Aircraft& own)
{
    base_heading_deg_ = own.kinematics.heading_deg;
    // start_s_ = now_s;   // World::sim_time_s otopilota iletilince
}

FlightCommand Snake::update(const Aircraft& own)
{
    FlightCommand command = own.command;

    // const double elapsed_s = now_s - start_s_;
    // const bool right_half = static_cast<int>(elapsed_s / HALF_PERIOD_S) % 2 == 0;
    // if (right_half)
    // {
    //     command.set_heading_deg(base_heading_deg_ + AMPLITUDE_DEG);
    // }
    // else
    // {
    //     command.set_heading_deg(base_heading_deg_ - AMPLITUDE_DEG);
    // }

    return command;
}

// ========================================================================================================
// PURSUIT  —  pure, lag, lead takip (İSKELET)
//
//   phase()       moda göre PurePursuit, LagPursuit, LeadPursuit
//   update(own)   örnek: moda göre yön
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

    // const double bearing_deg = own.picture.bearing_deg;
    // // Düşman bana göre saat yönünde kayıyorsa (LOS hızı +) önü bearing'in + tarafında, gerisi − tarafında.
    // const double ahead_sign = (own.picture.los_rate_deg_s >= 0.0) ? 1.0 : -1.0;
    //
    // switch (mode_)
    // {
    // case PursuitMode::Pure:
    //     command.set_heading_deg(bearing_deg);
    //     break;
    //
    // case PursuitMode::Lag:
    //     command.set_heading_deg(bearing_deg - ahead_sign * LAG_DEG);
    //     break;
    //
    // case PursuitMode::Lead:
    // {
    //     // Öncü açı: düşmanın LOS'a dik hızını benim hızımla karşıla (sin(lead) = V_düşman × sin(TA) / V_ben).
    //     // Düşmanın hızı picture'da yok; enemy.kinematics.tas_kt gerekir (konuşulacak).
    //     const double lead_deg = 20.0;
    //     command.set_heading_deg(bearing_deg + ahead_sign * lead_deg);
    //     break;
    // }
    // }

    return command;
}
