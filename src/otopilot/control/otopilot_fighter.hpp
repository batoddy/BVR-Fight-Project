#pragma once

#include "control/maneuver.hpp"
#include "model/aircraft.hpp"

// Fighter'ın taktiği: commit → MAR'a FIRE_BEFORE_MAR_NM kala at → crank ile füzeyi destekle →
// MAR'a girince out → kaçış rotasında kal.
enum class FighterState
{
    Commit,         // düşmana dön ve yaklaş; MAR'a az kalınca at
    Fire,           // tek step: füze at
    Crank,          // füzemi destekle; MAR'a girince out
    Out,            // kaç; düşman arkada kalınca biter
    HoldAfterOut,   // kaçış rotasını koru
};

// Durum makinesi. Her step: o durumun manevrası uçulur, sonra çıkış koşullarına bakılır.
// Geçişte yeni manevranın enter()'ı çağrılır; yeni manevra bir sonraki step'te uçulur.
class OtopilotFighter
{
public:
    // Yazdıkları: own.command, own.phase, (geçişte) atış emri.
    void step(Aircraft& own, const Aircraft& enemy);

private:
    static constexpr double FIRE_BEFORE_MAR_NM = 5.0;    // NM, MAR'a bu kadar kala at
    static constexpr double AFTER_OUT_THROTTLE = 0.45;   // 0..1, out bitince gaz (askeri güç altı)

    // Yeni duruma geç; manevranın enter()'ı geçiş anında bir kez çağrılır.
    void go_to(FighterState next, Maneuver& maneuver, const Aircraft& own);

    FighterState state_ = FighterState::Commit;   // ilk manevra In: enter()'ı yok
    Maneuvers maneuvers_;
};
