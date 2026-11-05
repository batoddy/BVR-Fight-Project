#include "control/maneuver.hpp"

#include <cmath>

#include "common/angles.hpp"

// ========================================================================================================
// ORTAK
//
//   Maneuvers::fly(own, maneuver)   değişen manevraya giriş yapar, ardından fazı ve komutu yazar
// ========================================================================================================

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
// FIRE  —  atış anı; atış emrini otopilot verir (own.weapons.fire)
//
//   update(own)   komut değişmez
// ========================================================================================================

FlightCommand Fire::update(const Aircraft& own)
{
    return own.command;
}

// ========================================================================================================
// OUT  —  düşmana kuyruk dön, alçal, tam art yakıcı; düşman arkada kalınca biter
//
//   enter(own)    girişte bir kez: hedef irtifayı kilitler (DESCEND_FT alçal, FLOOR_FT altına inme)
//   update(own)   her step: bearing − 180'e dön, hedef irtifaya in, tam gaz, out g'si
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
           .set_g_limit(own.profile.out_g);   // NEZ/MAR ile aynı g
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
//   update(own)   yön = düşmanın bearing'i; irtifa ve gaz aynı
// ========================================================================================================

FlightCommand Commit::update(const Aircraft& own)
{
    FlightCommand command = own.command;   // irtifa ve gaz aynı; yalnız yön değişir
    command.set_heading_deg(own.picture.bearing_deg);
    return command;
}


// ========================================================================================================
// Recommit  —  düşmana dönüp atış açısına gel
//
//   recommit_side_for(own)     kısa dönüş tarafı: düşman sağımdaysa (ATA ≥ 0) sağa, solumdaysa sola
//   enter(own)                 girişte bir kez: tarafı seçer (recommit_side_for)
//   update(own)                yön = bearing ∓ recommit_fire_ata_deg (Right −, Left +) + gecikme payı; irtifa ve gaz aynı
//   status(own)              atış açısına geldiyse (|ATA| ≤ recommit_fire_ata_deg + DONE_TOLERANCE_DEG) Done
// ========================================================================================================

// Dönüş, hesaplardaki gibi kısa taraftan: düşman hangi tarafımdaysa o tarafa dönerim.
ManeuverSide recommit_side_for(const Aircraft& own)
{
    if (own.picture.ata_deg >= 0.0)
    {
        return ManeuverSide::Right;
    }
    return ManeuverSide::Left;
}

// Taraf girişte bir kez seçilir.
void Recommit::enter(const Aircraft& own)
{
    side_ = recommit_side_for(own);
}

// Sağa dönüşte düşman burnun sağında kalır (ATA +), sola dönüşte solunda (ATA −). ATA = bearing − heading.
// Bearing kaydıkça uçak geride kalır; hedef, bu gecikme kadar ileri alınır (LOS_RATE_KP × HEADING_LAG_S × LOS hızı).
FlightCommand Recommit::update(const Aircraft& own)
{
    const double fire_ata_deg = own.profile.recommit_fire_ata_deg;

    // Atış açısına gelene kadar daha içeri hedefle ki sert dönüş 31°'den önce bitmesin; gelince atış açısını komut et.
    double target_ata_deg = fire_ata_deg - TURN_INSIDE_DEG;   // 30 − 10 = 20
    if (status(own) == ManeuverStatus::Done)
    {
        target_ata_deg = fire_ata_deg;
    }

    const double lead_deg = LOS_RATE_KP * HEADING_LAG_S * own.picture.los_rate_deg_s;

    FlightCommand command = own.command;
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
//   update(own)                yön = bearing ± (gimbal − OFFSET_DEG) (Right +, Left −) + gecikme payı; irtifa ve gaz aynı
//   in_radar_sight(own)        düşman radarın görüş konisinde mi (|ATA| < gimbal)
// ========================================================================================================

ManeuverSide crank_side_for(const Aircraft& own)
{
    if (own.picture.ta_deg >= 0.0)
    {
        return ManeuverSide::Left;    // düşmanın burnunun sağındayım: sola dönerim
    }
    return ManeuverSide::Right;       // düşmanın burnunun solundayım: sağa dönerim
}

// Taraf girişte bir kez seçilir.
void Crank::enter(const Aircraft& own)
{
    side_ = crank_side_for(own);
}

// Sağa crank'te düşman burnun solunda kalır (ATA −), sola crank'te sağında (ATA +). ATA = bearing − heading.
// Bearing kaydıkça uçak geride kalır; hedef, bu gecikme kadar ileri alınır (LOS_RATE_KP × HEADING_LAG_S × LOS hızı).
FlightCommand Crank::update(const Aircraft& own)
{
    const double limit_deg = own.profile.radar_gimbal_deg - OFFSET_DEG;
    const double lead_deg = LOS_RATE_KP * HEADING_LAG_S * own.picture.los_rate_deg_s;

    FlightCommand command = own.command;   // irtifa ve gaz aynı; yalnız yön değişir
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
//   update(own)   yön = bearing ∓ 90 (taraf burnuma göre); irtifa ve gaz aynı
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

    FlightCommand command = own.command;   // irtifa ve gaz aynı; yalnız yön değişir
    command.set_heading_deg(beam_heading_deg);
    return command;
}
