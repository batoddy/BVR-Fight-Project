#pragma once

#include "control/maneuvers/maneuver.hpp"

// PURSUIT  —  pure, lag, lead ve collision takip

enum class PursuitMode
{
    Pure,        // burun düşmanda; bugünkü Commit ile aynı
    Lag,         // burun düşmanın gerisinde: menzil yavaş kapanır, aşma riski az
    Lead,        // burun düşmanın önünde: menzil hızlı kapanır
    Collision,   // LOS hızını sıfırla: sabit bearing ile çarpışma (kesişme) rotası
};

// Tek sınıf, dört mod: maneuvers.pursuit.set(PursuitMode::Lag) sonra fly.
class Pursuit : public Maneuver
{
public:
    void set(PursuitMode mode, double throttle = throttle::DEFAULT);   // fly'dan önce çağrılır

    Phase phase() const override;                         // moda göre PurePursuit, LagPursuit, LeadPursuit, Collision
    FlightCommand update(const Aircraft& own) override;   // Pure bearing; Lag LOS yönünün gerisine LAG_DEG; Lead önüne LEAD_DEG; LOS dönmüyorsa pure. Collision: mevcut yöne KP_S × LOS hızı
    ManeuverStatus status(const Aircraft& own) const override; // ölçülen yön hedef yöne yakınsa Satisfied; Collision'da menzil kapanıyor ve LOS hızı tolerans içindeyse

private:
    static constexpr double LAG_DEG = 15.0;               // derece, Lag'de burnun düşmanın gerisinde kalacağı açı
    static constexpr double LEAD_DEG = 20.0;              // derece, Lead'de LOS hareketi yönündeki sabit öncü açı
    static constexpr double KP_S = 10.0;                  // s, Collision'da LOS hızını öncü açıya çeviren kazanç; simülasyonda ayarlanacak
    static constexpr double LOS_RATE_TOLERANCE_DEG_S = 0.1; // derece/s, Collision'da sabit yatay bearing toleransı
    static constexpr double G_LIMIT = 4.0;
    static constexpr double ANGLE_TOLERANCE_DEG = 2.0;

    double target_heading_deg(const Aircraft& own) const;  // derece, ilgili modun güncel hedef yönü

    PursuitMode mode_ = PursuitMode::Pure;
    double throttle_ = throttle::DEFAULT;
};
