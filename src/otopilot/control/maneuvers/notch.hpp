#pragma once

#include "control/maneuvers/maneuver.hpp"

// NOTCH  —  füzeye dik uç

// Beam'in füzeye göre olanı: bana gelen füzenin (own.picture.threat) 3/9 hattı; füze yoksa düşman uçağın.
class Notch : public Maneuver
{
public:
    Phase phase() const override { return Phase::Notch; }
    FlightCommand update(const Aircraft& own) override;   // yön = tehdidin bearing'i ∓ 90, kısa dönüş; irtifa aynı
    ManeuverStatus status(const Aircraft& own) const override;   // tehdit ATA ±90°'ye yakınsa Satisfied, değilse Running

private:
    static constexpr double THROTTLE = throttle::DEFAULT;
    static constexpr double G_LIMIT = 4.0;
    static constexpr double ANGLE_TOLERANCE_DEG = 2.0;
};
