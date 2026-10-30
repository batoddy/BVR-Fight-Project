#include "control/maneuver.hpp"

#include <cmath>

#include "common/angles.hpp"

void fly(Aircraft& own, Maneuver& maneuver)
{
    own.phase = maneuver.phase();
    own.command = maneuver.update(own);
}

FlightCommand Hold::update(const Aircraft& own)
{
    FlightCommand command = own.command;   // gaz aynı kalır
    command.set_heading_deg(own.kinematics.heading_deg)
           .set_altitude_ft(own.kinematics.altitude_ft)
           .clear_g_limit();
    return command;
}

FlightCommand Fire::update(const Aircraft& own)
{
    return own.command;
}

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

FlightCommand In::update(const Aircraft& own)
{
    FlightCommand command = own.command;   // irtifa ve gaz aynı; yalnız yön değişir
    command.set_heading_deg(own.picture.bearing_deg);
    return command;
}

CrankSide crank_side_for(const Aircraft& own)
{
    if (own.picture.ata_deg >= 0.0)
    {
        return CrankSide::Left;    // düşman sağımda: sola dönerim, sağımda kalır
    }
    return CrankSide::Right;       // düşman solumda: sağa dönerim, solumda kalır
}

// Taraf girişte bir kez: düşman hangi taraftaysa o tarafta kalır.
void Crank::enter(const Aircraft& own)
{
    side_ = crank_side_for(own);
}

// Düşmanın burnuma göre tutulacağı açı: gimbal sınırının OFFSET_DEG içi.
//   Sola crank: düşman burnumun sağında kalır (+).  Sağa crank: solunda kalır (−).
double Crank::target_ata_deg(const Aircraft& own) const
{
    const double limit_deg = own.profile.radar_gimbal_deg - OFFSET_DEG;
    if (side_ == CrankSide::Left)
    {
        return limit_deg;
    }
    return -limit_deg;
}

// P kontrolü:
//   hata    = düşmanın şu anki ATA'sı − tutulması gereken ATA
//   düzeltme = KP × hata, en fazla ±MAX_CORRECTION_DEG
//   yeni yön = şu anki yön + düzeltme
FlightCommand Crank::update(const Aircraft& own)
{
    const double error_deg = wrap_180(own.picture.ata_deg - target_ata_deg(own));

    double correction_deg = KP * error_deg;
    if (correction_deg > MAX_CORRECTION_DEG)
    {
        correction_deg = MAX_CORRECTION_DEG;
    }
    if (correction_deg < -MAX_CORRECTION_DEG)
    {
        correction_deg = -MAX_CORRECTION_DEG;
    }

    FlightCommand command = own.command;   // irtifa ve gaz aynı; yalnız yön değişir
    command.set_heading_deg(own.kinematics.heading_deg + correction_deg);
    return command;
}

// Yerine oturdu mu: düşmanın ATA'sı hedef ATA'dan en fazla tolerance_deg sapıyor.
bool Crank::in_position(const Aircraft& own, double tolerance_deg) const
{
    const double error_deg = wrap_180(own.picture.ata_deg - target_ata_deg(own));
    return std::abs(error_deg) <= tolerance_deg;
}

// Düşmana dik uç: bearing'in 90° yanı.
// NOT: taraf kuzeye göre seçiliyor; doğrusu şu anki yöne yakın olan tarafı seçmek (maneuver.hpp).
FlightCommand Beam::update(const Aircraft& own)
{
    const double bearing_deg = own.picture.bearing_deg;

    double beam_heading_deg = bearing_deg + 90.0;   // düşman batı yarısındaysa
    if (bearing_deg > 0.0)
    {
        beam_heading_deg = bearing_deg - 90.0;      // düşman doğu yarısındaysa
    }

    FlightCommand command = own.command;   // irtifa ve gaz aynı; yalnız yön değişir
    command.set_heading_deg(beam_heading_deg);
    return command;
}
