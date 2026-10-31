#pragma once

#include "control/maneuver.hpp"
#include "model/aircraft.hpp"

// Hostile'ın taktiği: fighter'a doğru gel → fighter kendi MAR'ına girer girmez at → out → kaçış rotasında kal.
enum class HostileState
{
    Commit,         // düşmana dön ve yaklaş; düşman kendi MAR'ına girince at
    Fire,           // tek step: füze at
    Out,            // kaç; düşman arkada kalınca biter
    HoldAfterOut,   // kaçış rotasını koru
};

// Durum makinesi. Her step: o durumun manevrası uçulur, sonra çıkış koşullarına bakılır.
// Geçişte yeni manevranın enter()'ı çağrılır; yeni manevra bir sonraki step'te uçulur.
class OtopilotHostile
{
public:
    // Yazdıkları: own.command, own.phase, (geçişte) atış emri.
    void step(Aircraft& own, const Aircraft& enemy);

private:
    static constexpr double DEFAULT_THROTTLE = 0.45;   // 0..1, out bitince gaz (askeri güç altı)

    // Yeni duruma geç; manevranın enter()'ı geçiş anında bir kez çağrılır.
    void go_to(HostileState next, Maneuver& maneuver, const Aircraft& own);

    HostileState state_ = HostileState::Commit;   // ilk manevra In: enter()'ı yok
    Maneuvers maneuvers_;
};
