#pragma once

#include "control/maneuver.hpp"
#include "model/aircraft.hpp"

// Fighter akışı: ilk yaklaşma ve atış → ilk destek → kaçış → yeniden yaklaşma →
// atış açısına gelince ikinci atış → ikinci destek → son kaçış → rotayı koru.
enum class FighterState
{
    Commit = 0,         // Hold: düz uç; 35 NM içinde ilk atışa geç
    FirstShot,      // Fire: tek step ilk atış emri
    FirstSupport,   // Crank: DOR'a girince ilk kaçışa geç
    FirstEscape,    // Out: düşman arkada kalınca yeniden yaklaş
    Recommit,       // Recommit: atış açısına gelince ikinci atışa geç
    SecondShot,     // Fire: tek step ikinci atış emri
    SecondSupport,  // Crank: MAR'a girince son kaçışa geç
    FinalEscape,    // Out: düşman arkada kalınca rotayı koru
    Departure,      // Hold: çıkış rotasını koru
    Hold,
};

// Durum makinesi. Her step: o durumun manevrası uçulur, sonra çıkış koşullarına bakılır.
// go_to yalnız state'i değiştirir; Maneuvers::fly değişen manevranın enter()'ını ilk kullanımda çağırır.
class OtopilotFighter
{
public:
    // Yazdıkları: own.command, own.phase, (geçişte) atış emri.
    void step(Aircraft& own, const Aircraft& enemy);

private:
    static constexpr double DEFAULT_THROTTLE = 0.45;   // 0..1, out bitince gaz (askeri güç altı)
    static constexpr double FIRST_SHOT_RANGE_NM = 35.0;   // NM, ilk atışa geçiş menzili

    // Yalnız yeni duruma geç; manevra o durumun bloğunda seçilir.
    void go_to(FighterState next);

    FighterState state_ = FighterState::Commit;
    Maneuvers maneuvers_;
};
