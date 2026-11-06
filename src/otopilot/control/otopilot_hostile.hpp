#pragma once

#include "control/maneuver.hpp"
#include "control/otopilot_state.hpp"
#include "model/aircraft.hpp"

// Durum makinesi. Her step: o durumun manevrası uçulur, sonra çıkış koşullarına bakılır.
// go_to yalnız state'i değiştirir; Maneuvers::fly değişen manevranın enter()'ını ilk kullanımda çağırır.
// Şu an deneme: yalnız Commit uçulur, atış yapılmaz.
class OtopilotHostile
{
public:
    // Yazdıkları: own.command, own.phase, (geçişte) atış emri.
    void step(Aircraft& own, const Aircraft& enemy);

private:
    // Yalnız yeni duruma geç; manevra o durumun bloğunda seçilir.
    void go_to(OtopilotState next);

    OtopilotState state_ = OtopilotState::Commit;
    Maneuvers maneuvers_;
};
