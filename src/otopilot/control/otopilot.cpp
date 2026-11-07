// #include "control/otopilot.hpp"

// #include <string>

// // Gazı ve g sınırını manevralar yazar; atış durumları yalnız atış emri verir, manevra uçmaz.
// // Atış emri kabul edilmezse (önceki emir henüz uçmadı) aynı durumda kalınır, sonraki step tekrar denenir.
// void Otopilot::step(Aircraft& own, const Aircraft& enemy)
// {
//     (void)enemy;

//     const MissileProfile& weapon = own.profile.main_weapon;

//     if (react_to_threat(own))   // K4 akışı kesti: yeni durum sonraki step uçulur
//     {
//         return;
//     }

//     switch (state_)
//     {
//     case OtopilotState::Commit:
//     {
//         maneuvers_.fly(own, maneuvers_.commit);
//         const Decision<FirstShot> first_shot = decide::first_shot(own, mission_);
//         if (first_shot.choice == FirstShot::Fire)
//         {
//             record(own, "K1 atış: ateş", first_shot.reason);
//             go_to(OtopilotState::Shot);
//         }
//         break;
//     }

//     case OtopilotState::Shot:
//         if (own.weapons.fire(weapon))
//         {
//             go_to(OtopilotState::Support);
//         }
//         break;

//     case OtopilotState::Support:
//     {
//         maneuvers_.fly(own, maneuvers_.crank);
//         const Decision<SupportLimit> support = decide::support_limit(own, mission_);
//         if (support.choice == SupportLimit::Escape)
//         {
//             record(own, "K3 destek sonu: çık", support.reason);
//             go_to(OtopilotState::Escape);
//         }
//         else if (support.choice == SupportLimit::FireAndEscape)
//         {
//             record(own, "K3 destek sonu: at ve çık", support.reason);
//             go_to(OtopilotState::EscapeShot);
//         }
//         break;
//     }

//     case OtopilotState::EscapeShot:
//         if (own.weapons.fire(weapon) || own.weapons.missiles_left() == 0)   // füze kalmadıysa atmadan çık
//         {
//             go_to(OtopilotState::Escape);
//         }
//         break;

//     case OtopilotState::Escape:
//         maneuvers_.fly(own, maneuvers_.out);
//         if (maneuvers_.out.status(own) == ManeuverStatus::Done)
//         {
//             after_escape(own, "K5 Out bitti");
//         }
//         break;

//     case OtopilotState::Recommit:
//         maneuvers_.fly(own, maneuvers_.recommit);
//         if (maneuvers_.recommit.status(own) == ManeuverStatus::Done)
//         {
//             go_to(OtopilotState::Commit);   // atışa yine K1 karar verir
//         }
//         break;

//     case OtopilotState::Departure:
//         maneuvers_.fly(own, maneuvers_.hold);
//         break;

//     case OtopilotState::CounterShot:
//         if (own.weapons.fire(weapon))
//         {
//             go_to(after_counter_shot_);
//         }
//         break;

//     case OtopilotState::Defense:
//     {
//         maneuvers_.fly(own, defense_maneuver());
//         if (own.picture.threat == nullptr)   // bana gelen füze kalmadı
//         {
//             after_escape(own, "K5 savunma bitti");
//             break;
//         }
//         const Decision<ThreatResponse> defense = decide::defense(own, mission_);
//         if (static_cast<int>(defense.choice) > static_cast<int>(defense_))   // savunma yalnız acilleşir: Beam → Drag → Notch
//         {
//             defense_ = defense.choice;
//             record(own, defense_name(defense_), defense.reason);
//         }
//         break;
//     }
//     }
// }

// void Otopilot::go_to(OtopilotState next)
// {
//     state_ = next;
// }

// // K4 her uçuş durumunda sorulur; tek step'lik atış durumları ve zaten Defense'teyken sorulmaz.
// bool Otopilot::react_to_threat(Aircraft& own)
// {
//     const bool firing = state_ == OtopilotState::Shot || state_ == OtopilotState::EscapeShot
//                         || state_ == OtopilotState::CounterShot;
//     if (firing || state_ == OtopilotState::Defense)
//     {
//         return false;
//     }

//     const Decision<ThreatResponse> response = decide::threat_response(own, mission_);
//     switch (response.choice)
//     {
//     case ThreatResponse::Continue:
//         return false;
//     case ThreatResponse::CounterFire:
//         record(own, "K4 füze geliyor: karşı atış", response.reason);
//         after_counter_shot_ = state_;                       // destek, Out ya da ayrılışta aynı duruma dönülür
//         if (state_ == OtopilotState::Commit || state_ == OtopilotState::Recommit)
//         {
//             after_counter_shot_ = OtopilotState::Support;   // yaklaşıyordum: füzeyi desteklerim
//         }
//         go_to(OtopilotState::CounterShot);
//         return true;
//     default:
//         defense_ = response.choice;
//         record(own, defense_name(defense_), response.reason);
//         go_to(OtopilotState::Defense);
//         return true;
//     }
// }

// void Otopilot::after_escape(Aircraft& own, const char* text)
// {
//     const Decision<AfterEscape> after = decide::after_escape(own, mission_);
//     switch (after.choice)
//     {
//     case AfterEscape::Wait:   // aynı durumda kalınır, sonraki step yeniden sorulur
//         record(own, (std::string(text) + ": bekle").c_str(), after.reason);
//         break;
//     case AfterEscape::Recommit:
//         record(own, (std::string(text) + ": Recommit").c_str(), after.reason);
//         go_to(OtopilotState::Recommit);
//         break;
//     case AfterEscape::Depart:
//         record(own, (std::string(text) + ": ayrıl").c_str(), after.reason);
//         go_to(OtopilotState::Departure);
//         break;
//     }
// }

// Maneuver& Otopilot::defense_maneuver()
// {
//     switch (defense_)
//     {
//     case ThreatResponse::Drag: return maneuvers_.drag;
//     case ThreatResponse::Notch: return maneuvers_.notch;
//     default: return maneuvers_.beam;
//     }
// }

// const char* Otopilot::defense_name(ThreatResponse response)
// {
//     switch (response)
//     {
//     case ThreatResponse::Drag: return "K4 savunma: Drag";
//     case ThreatResponse::Notch: return "K4 savunma: Notch";
//     default: return "K4 savunma: Beam";
//     }
// }

// void Otopilot::record(Aircraft& own, const char* text, const char* reason) const
// {
//     own.decision = std::string(text) + " — " + reason + " (" + goal_name(mission_.goal) + " / " + risk_name(mission_.risk) + ")";
// }
