#pragma once

#include "control/maneuvers/maneuver.hpp"

// DRAG  —  füzeye sırt dön

// Out'un füzeye göre olanı: füzeyi tam arkaya al, kapanma hızını düşür; füze yoksa düşman uçağı arkaya alır.
class Drag : public Maneuver
{
public:
    Phase phase() const override { return Phase::Drag; }
    FlightCommand update(const Aircraft& own) override;   // yön = tehdidin bearing'i − 180; g out_g; irtifa aynı
    ManeuverStatus status(const Aircraft& own) const override;   // tehdit ATA ±180°'ye yakınsa Satisfied, değilse Running

private:
    static constexpr double THROTTLE = throttle::MRT;            // P-1290: drag askeri güçle
    static constexpr double ANGLE_TOLERANCE_DEG = 2.0;
};
