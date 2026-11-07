#pragma once

#include "control/decisions.hpp"
#include "control/maneuver.hpp"
#include "control/otopilot_state.hpp"
#include "model/aircraft.hpp"
#include "model/mission.hpp"

// Durum makinesi; fighter ve hostile aynı sınıfı kendi görev konseptiyle kullanır. Kurallar: control/KARAR_TASARIMI.md
// Her step: önce kesintiler (ABORT, tehdit), sonra o fazın fonksiyonu: manevrasını uçar, kararına göre geçiş yapar.
// Geçiş yalnız go_to ile. Kesinti geçişinde yeni faz aynı step uçulur; fazın kendi geçişinde sonraki step.
class Otopilot
{
public:
    explicit Otopilot(Mission mission) : mission_(mission) {}
    void step(Aircraft& own, const Aircraft& enemy);   // yazdıkları: own.command, own.phase, own.decision, atış emri

private:
    void step_commit(Aircraft& own);
    void step_shot(Aircraft& own);
    void step_support(Aircraft& own);
    void step_trade(Aircraft& own);
    void step_defend(Aircraft& own);
    void step_out(Aircraft& own);
    void step_cold(Aircraft& own);
    void step_recommit(Aircraft& own);
    void step_chase(Aircraft& own);
    void step_abort(Aircraft& own);

    void check_abort(Aircraft& own);                   // kesinti: ABORT dışındaki her fazda
    void react_to_threat(Aircraft& own);               // kesinti: Commit, Support, Out, Cold, Recommit, Chase'te
    void reassess(Aircraft& own);                      // REASSESS: anlık düğüm, yeni fazı seçer
    void start_defense(Aircraft& own, const char* text);   // savunma tipini seçip Defend'e geçer
    void update_defense(Aircraft& own);                // savunma yalnız acilleşir: Beam → Drag → Notch
    Maneuver& defense_maneuver();                      // defense_'e göre Beam / Drag / Notch
    Maneuver& support_maneuver(const Aircraft& own);   // düşman cold ise kovalama, değilse crank
    void go_to(OtopilotState next);                    // yalnız durumu değiştirir
    void record(Aircraft& own, const char* text, const char* reason) const;   // own.decision = "text — reason (görev)"

    Mission mission_;
    OtopilotState state_ = OtopilotState::Commit;
    Defense defense_ = Defense::Beam;                  // Defend'te uçulan savunma
    bool gave_up_chase_ = false;                       // kovalamadan vazgeçtim; düşman hot'a dönünce sıfırlanır
    Maneuvers maneuvers_;
};
