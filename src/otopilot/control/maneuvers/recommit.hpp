#pragma once

#include "control/maneuvers/maneuver.hpp"

// RECOMMIT  —  düşmana dönüp atış açısına gel

// Hedef tam atış açısı (30°) olsaydı yön hatası küçüldükçe dönüş yumuşar, 40°'den 30°'ye iniş uzardı.
// Bu yüzden ilk varışa kadar hedef TURN_INSIDE_DEG içeride (20°) tutulur, varınca 30°'ye alınır ve orada kalınır.
class Recommit : public Maneuver
{
public:
    Phase phase() const override { return Phase::Recommit; }
    void enter(const Aircraft& own) override;                     // taraf: düşman sağımdaysa Right, solumdaysa Left; varış bayrağı false
    FlightCommand update(const Aircraft& own) override;           // yön = bearing ∓ hedef ATA; irtifa aynı, g out_g
    ManeuverStatus status(const Aircraft& own) const override;    // |ATA| ≤ recommit_fire_ata_deg + DONE_TOLERANCE_DEG: Done
    ManeuverSide side() const { return side_; }                   // girişte seçilen taraf

private:
    static constexpr double TURN_INSIDE_DEG = 10.0;               // derece, varana kadar hedefin atış açısından içeride tutulduğu açı
    static constexpr double DONE_TOLERANCE_DEG = 1.0;             // derece, |ATA| atış açısını bu kadar aşsa da varılmış sayılır
    static constexpr double THROTTLE = throttle::MAX_THRUST;

    ManeuverSide side_ = ManeuverSide::Left;                      // enter()'da seçilir
    bool has_reached_ = false;                                    // atış açısına bir kez varıldı mı
};
