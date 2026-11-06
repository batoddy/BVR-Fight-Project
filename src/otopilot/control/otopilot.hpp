#pragma once

#include "control/decisions.hpp"
#include "control/maneuver.hpp"
#include "control/otopilot_state.hpp"
#include "model/aircraft.hpp"
#include "model/mission.hpp"

// Durum makinesi; fighter ve hostile aynı sınıfı kendi görev konseptiyle kullanır.
// Her step: o durumun manevrası uçulur, sonra çıkış koşullarına bakılır.
// go_to yalnız state'i değiştirir; Maneuvers::fly değişen manevranın enter()'ını ilk kullanımda çağırır.
// Akış: Commit → K1'de atış → Crank → K3'te Out (İmha/Riskli: MAR'da bir atış daha) → K5: Recommit → Commit ... / Departure.
// K4: atış ve savunma dışındaki her durumda bana füze gelirse karşı atış ya da savunma (Defense); tehdit bitince K5.
class Otopilot
{
public:
    explicit Otopilot(Mission mission) : mission_(mission) {}
    void step(Aircraft& own, const Aircraft& enemy);   // yazdıkları: own.command, own.phase, own.decision, atış emri

private:
    void go_to(OtopilotState next);                    // yalnız durumu değiştirir; manevra o durumun bloğunda seçilir
    void record(Aircraft& own, const char* text, const char* reason) const;   // own.decision = "text — reason (görev)"
    bool react_to_threat(Aircraft& own);               // K4: akışı keserse true
    void after_escape(Aircraft& own, const char* text); // K5: Recommit ya da Departure
    Maneuver& defense_maneuver();                      // defense_'e göre Beam / Drag / Notch
    static const char* defense_name(ThreatResponse response);   // karar kaydı metni

    Mission mission_;
    OtopilotState state_ = OtopilotState::Commit;
    OtopilotState after_counter_shot_ = OtopilotState::Support;   // karşı atıştan sonra dönülecek durum
    ThreatResponse defense_ = ThreatResponse::Beam;               // Defense'te uçulan savunma
    Maneuvers maneuvers_;
};
