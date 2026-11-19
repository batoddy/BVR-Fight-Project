#pragma once

#include "control/maneuvers/maneuver.hpp"

// FLY HEADING  —  verilen sabit yöne uç

// Flow cold, Reset ve Departure için: yön taktikten gelir.
class FlyHeading : public Maneuver
{
public:
    Phase phase() const override { return Phase::FlyHeading; }
    void set(double heading_deg);                         // fly'dan önce çağrılır; yönü ±180'e sarıp saklar
    FlightCommand update(const Aircraft& own) override;   // yön = heading_deg_; irtifa aynı
    ManeuverStatus status(const Aircraft& own) const override; // ölçülen yön verilen açıya yakınsa Satisfied

private:
    static constexpr double THROTTLE = throttle::DEFAULT;
    static constexpr double G_LIMIT = 4.0;
    static constexpr double ANGLE_TOLERANCE_DEG = 2.0;

    double heading_deg_ = 0;                              // derece ±180
};
