#pragma once

#include "control/maneuvers/maneuver.hpp"

// OFFSET  —  düşman yönünden verilen açı kadar yana uç

// Açı ve taraf taktikten gelir: maneuvers.offset.set(40.0, ManeuverSide::Left) sonra fly.
class Offset : public Maneuver
{
public:
    Phase phase() const override { return Phase::Offset; }
    void set(double offset_deg, ManeuverSide side);       // fly'dan önce çağrılır; açı sonlu ve 0..180 derece olmalıdır
    FlightCommand update(const Aircraft& own) override;   // yön = bearing ± offset_deg_ (Right +, Left −)
    ManeuverStatus status(const Aircraft& own) const override; // seçilen tarafta hedef ATA'ya yakınsa Satisfied

private:
    static constexpr double THROTTLE = throttle::DEFAULT;
    static constexpr double G_LIMIT = 4.0;
    static constexpr double ANGLE_TOLERANCE_DEG = 2.0;

    double offset_deg_ = 0;                               // derece, burnun düşman yönünden sapması
    ManeuverSide side_ = ManeuverSide::Left;
};
