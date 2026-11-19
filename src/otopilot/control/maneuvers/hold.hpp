#pragma once

#include "control/maneuvers/maneuver.hpp"

// HOLD  —  girişteki yön ve irtifayı koru

class Hold : public Maneuver
{
public:
    Phase phase() const override { return Phase::Hold; }
    void enter(const Aircraft& own) override;             // ölçülen yön ve irtifayı bir kez kilitler
    FlightCommand update(const Aircraft& own) override;   // kilitlenen yön ve irtifaya geri döner
    ManeuverStatus status(const Aircraft& own) const override; // yön ve irtifa tolerans içindeyse Satisfied

private:
    static constexpr double THROTTLE = throttle::DEFAULT;
    static constexpr double G_LIMIT = 4.0;
    static constexpr double ANGLE_TOLERANCE_DEG = 2.0;
    static constexpr double ALTITUDE_TOLERANCE_FT = 100.0;

    double target_heading_deg_ = 0;                       // derece, girişteki yön | enter
    double target_altitude_ft_ = 0;                       // ft, girişteki irtifa | enter
    bool has_target_ = false;
};
