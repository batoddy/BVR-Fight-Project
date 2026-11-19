#pragma once

#include "control/maneuvers/maneuver.hpp"

// COMMIT  —  düşmana doğru düz uç (saf takip)

class Commit : public Maneuver
{
public:
    Phase phase() const override { return Phase::Commit; }
    FlightCommand update(const Aircraft& own) override;   // yön = düşmanın bearing'i; irtifa aynı
    ManeuverStatus status(const Aircraft& own) const override; // burun hedef yönüne tolerans içinde bakıyorsa Satisfied

private:
    static constexpr double THROTTLE = throttle::DEFAULT;
    static constexpr double G_LIMIT = 4.0;                // g, yumuşak dönüş: hız ve irtifa korunur
    static constexpr double ANGLE_TOLERANCE_DEG = 2.0;
};
