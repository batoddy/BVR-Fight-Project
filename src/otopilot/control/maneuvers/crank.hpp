#pragma once

#include "control/maneuvers/maneuver.hpp"

// CRANK  —  düşmanı radar gimbal sınırının biraz içinde tutarak yanal kaçış

// Taraf girişte düşmanın gidişine göre seçilir (benim burnuma göre değil) ve crank boyunca sabit kalır.
class Crank : public Maneuver
{
public:
    Phase phase() const override { return Phase::Crank; }
    void enter(const Aircraft& own) override;                 // taraf: TA ≥ 0 (düşmanın burnunun sağındayım) ise Left, değilse Right
    FlightCommand update(const Aircraft& own) override;       // yön = bearing ± (gimbal − OFFSET_DEG); irtifa aynı
    ManeuverStatus status(const Aircraft& own) const override; // seçilen tarafta hedef ATA tolerans içindeyse ve radar görüyorsa Satisfied
    bool in_radar_sight(const Aircraft& own) const;           // düşman radarın görüş konisinde mi (|ATA| < gimbal)
    ManeuverSide side() const { return side_; }               // girişte seçilen taraf

private:
    static constexpr double OFFSET_DEG = 3.0;                 // derece, gimbal sınırından içeri pay
    static constexpr double ANGLE_TOLERANCE_DEG = 2.0;        // derece, hedef ATA'dan izin verilen sapma
    static constexpr double THROTTLE = throttle::CRANK;
    static constexpr double G_LIMIT = 3.0;                    // g, yumuşak dönüş (flight_control: 3 g crank 50° ≈ 14.5 s)

    ManeuverSide side_ = ManeuverSide::Left;                  // enter()'da seçilir
};
