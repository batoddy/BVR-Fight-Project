#pragma once

#include "control/maneuver.hpp"
#include "control/otopilot_state.hpp"
#include "model/aircraft.hpp"

// Durum makinesi. Her step: o durumun manevrası uçulur, sonra çıkış koşullarına bakılır.
// go_to yalnız state'i değiştirir; Maneuvers::fly değişen manevranın enter()'ını ilk kullanımda çağırır.
// Akış: Commit → WEZ'de ilk atış → Crank → DOR'da Out → Recommit → ikinci atış → Crank → MAR'da Out → Departure.
// Commit'te atmadan MAR'a girersem doğrudan Out.
class OtopilotFighter
{
public:
    void step(Aircraft& own, const Aircraft& enemy);   // yazdıkları: own.command, own.phase, atış emri

private:
    void go_to(OtopilotState next);                    // yalnız durumu değiştirir; manevra o durumun bloğunda seçilir

    OtopilotState state_ = OtopilotState::Commit;
    Maneuvers maneuvers_;
};
