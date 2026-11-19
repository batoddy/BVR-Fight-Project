#pragma once

#include "control/maneuvers/maneuver.hpp"

// BEAM  —  düşman uçağa dik uç (3/9 hattı)

// Taraf girişte burnuma göre seçilir ve sabit kalır; dönüş hep 90°'den kısa. Füzeye göre olanı Notch.
class Beam : public Maneuver
{
public:
    Phase phase() const override { return Phase::Beam; }
    void enter(const Aircraft& own) override;                 // taraf: düşman sağımdaysa (ATA > 0) Left, solumdaysa Right
    FlightCommand update(const Aircraft& own) override;       // yön = bearing − 90 (Left) / + 90 (Right); irtifa aynı
    ManeuverStatus status(const Aircraft& own) const override; // seçilen tarafta ATA ±90°'ye yakınsa Satisfied
    ManeuverSide side() const { return side_; }               // girişte seçilen taraf

private:
    static constexpr double THROTTLE = throttle::DEFAULT;
    static constexpr double G_LIMIT = 4.0;                    // g, yumuşak dönüş
    static constexpr double ANGLE_TOLERANCE_DEG = 2.0;

    ManeuverSide side_ = ManeuverSide::Left;                  // enter()'da seçilir
};
