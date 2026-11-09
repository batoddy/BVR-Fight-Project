#include "control/otopilot.hpp"

#include <string>

#include "control/conditions.hpp"

namespace
{
    constexpr PursuitMode CHASE_MODE = PursuitMode::Pure;   // kovalama takibi: Pure, Lag ya da Lead
}

// Gazı ve g sınırını manevralar yazar; Shot yalnız atış emri verir, manevra uçmaz.
void Otopilot::step(Aircraft& own)
{
    if (condition::enemy_hot(own))
    {
        gave_up_chase_ = false;   // düşman döndü: kaçarsa yeniden kovalanabilir
    }

    if (state_ != OtopilotState::Abort)
    {
        check_abort(own);
    }
    react_to_threat(own);

    switch (state_)
    {
    case OtopilotState::Commit: step_commit(own); break;
    case OtopilotState::Shot: step_shot(own); break;
    case OtopilotState::Support: step_support(own); break;
    case OtopilotState::Trade: step_trade(own); break;
    case OtopilotState::Defend: step_defend(own); break;
    case OtopilotState::Out: step_out(own); break;
    case OtopilotState::Cold: step_cold(own); break;
    case OtopilotState::Recommit: step_recommit(own); break;
    case OtopilotState::Chase: step_chase(own); break;
    case OtopilotState::Abort: step_abort(own); break;
    }
}

// ========================================================================================================
// FAZLAR
// ========================================================================================================

void Otopilot::step_commit(Aircraft& own)
{
    maneuvers_.fly(own, maneuvers_.commit);
    const Decision<CommitChoice> decision = decide::commit(own);
    switch (decision.choice)
    {
    case CommitChoice::Continue:
        break;
    case CommitChoice::Fire:
        record(own, "COMMIT: ateş", decision.reason);
        go_to(OtopilotState::Shot);
        break;
    case CommitChoice::Reassess:
        record(own, "COMMIT", decision.reason);
        reassess(own);
        break;
    }
}

// Emir kabul edilmezse (önceki emir henüz uçmadı) aynı durumda kalınır; füze kalmadıysa atmadan geçilir.
void Otopilot::step_shot(Aircraft& own)
{
    if (own.weapons.fire(own.profile.main_weapon) || own.weapons.missiles_left() == 0)
    {
        go_to(OtopilotState::Support);
    }
}

void Otopilot::step_support(Aircraft& own)
{
    maneuvers_.fly(own, support_maneuver(own));
    const Decision<SupportChoice> decision = decide::support(own, mission_);
    switch (decision.choice)
    {
    case SupportChoice::Continue:
        break;
    case SupportChoice::SecondShot:
        record(own, "SUPPORT: 2. atış", decision.reason);
        go_to(OtopilotState::Shot);
        break;
    case SupportChoice::Out:
        record(own, "SUPPORT → OUT", decision.reason);
        go_to(OtopilotState::Out);
        break;
    case SupportChoice::Abort:
        record(own, "SUPPORT → ABORT (L&L)", decision.reason);
        go_to(OtopilotState::Abort);
        break;
    case SupportChoice::Reassess:
        record(own, "SUPPORT", decision.reason);
        reassess(own);
        break;
    }
}

void Otopilot::step_trade(Aircraft& own)
{
    maneuvers_.fly(own, maneuvers_.crank);
    const Decision<TradeChoice> decision = decide::trade(own, mission_);
    switch (decision.choice)
    {
    case TradeChoice::Continue:
        break;
    case TradeChoice::Support:
        record(own, "TRADE → SUPPORT", decision.reason);
        go_to(OtopilotState::Support);
        break;
    case TradeChoice::Defend:
        start_defense(own, decision.reason);
        break;
    }
}

// Tehdit bitene, güdümünü kaybedene ya da zararsız olana kadar savunmada kalınır; yetişemiyor olması yetmez.
void Otopilot::step_defend(Aircraft& own)
{
    maneuvers_.fly(own, defense_maneuver());
    if (!condition::has_guided_threat(own))
    {
        record(own, "DEFEND", "tehdit bitti");
        reassess(own);
        return;
    }
    if (condition::threat_harmless(own))
    {
        record(own, "DEFEND", "füze benden yavaş, recommit payı var");
        reassess(own);
        return;
    }
    update_defense(own);
}

void Otopilot::step_out(Aircraft& own)
{
    maneuvers_.fly(own, maneuvers_.out);
    if (maneuvers_.out.status(own) == ManeuverStatus::Done)
    {
        record(own, "OUT → COLD", "düşman arkada");
        go_to(OtopilotState::Cold);
    }
}

// Out'tan gelindiyse aynı out manevrası (art yakıcı) sürer; bekleme kararında hold.
void Otopilot::step_cold(Aircraft& own)
{
    const Decision<ColdChoice> decision = decide::cold(own, mission_, gave_up_chase_);
    if (decision.choice == ColdChoice::Hold)
    {
        maneuvers_.fly(own, maneuvers_.hold);
    }
    else
    {
        maneuvers_.fly(own, maneuvers_.out);
    }

    switch (decision.choice)
    {
    case ColdChoice::Run:
    case ColdChoice::Hold:
        break;
    case ColdChoice::Recommit:
        record(own, "COLD → RECOMMIT", decision.reason);
        go_to(OtopilotState::Recommit);
        break;
    case ColdChoice::Chase:
        record(own, "COLD → CHASE", decision.reason);
        go_to(OtopilotState::Chase);
        break;
    }
}

void Otopilot::step_recommit(Aircraft& own)
{
    maneuvers_.fly(own, maneuvers_.recommit);
    if (condition::in_dor(own))
    {
        record(own, "RECOMMIT → OUT", "dönerken DOR'a girildi");
        go_to(OtopilotState::Out);
        return;
    }
    if (maneuvers_.recommit.status(own) == ManeuverStatus::Done)
    {
        record(own, "RECOMMIT → COMMIT", "atış açısındayım");
        go_to(OtopilotState::Commit);
    }
}

void Otopilot::step_chase(Aircraft& own)
{
    maneuvers_.fly(own, chase_maneuver());
    const Decision<ChaseChoice> decision = decide::chase(own);
    switch (decision.choice)
    {
    case ChaseChoice::Continue:
        break;
    case ChaseChoice::Fire:
        record(own, "CHASE: ateş", decision.reason);
        go_to(OtopilotState::Shot);
        break;
    case ChaseChoice::Commit:
        record(own, "CHASE → COMMIT", decision.reason);
        go_to(OtopilotState::Commit);
        break;
    case ChaseChoice::Out:
        record(own, "CHASE → OUT", decision.reason);
        go_to(OtopilotState::Out);
        break;
    case ChaseChoice::GiveUp:
        record(own, "CHASE → COLD", decision.reason);
        gave_up_chase_ = true;
        go_to(OtopilotState::Cold);
        break;
    }
}

// Kalıcı: tehdit gelse de out sürer.
void Otopilot::step_abort(Aircraft& own)
{
    maneuvers_.fly(own, maneuvers_.out);
}

// ========================================================================================================
// KESİNTİLER VE REASSESS
// ========================================================================================================

void Otopilot::check_abort(Aircraft& own)
{
    const Decision<AbortChoice> decision = decide::abort(own);
    if (decision.choice == AbortChoice::Abort)
    {
        record(own, "ABORT", decision.reason);
        go_to(OtopilotState::Abort);
    }
}

void Otopilot::react_to_threat(Aircraft& own)
{
    const bool interruptible = state_ == OtopilotState::Commit || state_ == OtopilotState::Support
                               || state_ == OtopilotState::Out || state_ == OtopilotState::Cold
                               || state_ == OtopilotState::Recommit || state_ == OtopilotState::Chase;
    if (!interruptible)
    {
        return;
    }

    const Decision<ThreatResponse> decision = decide::threat_response(own, mission_);
    switch (decision.choice)
    {
    case ThreatResponse::Continue:
        break;
    case ThreatResponse::CounterFire:
        record(own, "TEHDİT: karşı atış", decision.reason);
        go_to(OtopilotState::Shot);
        break;
    case ThreatResponse::Trade:
        record(own, "TEHDİT → TRADE", decision.reason);
        go_to(OtopilotState::Trade);
        break;
    case ThreatResponse::Defend:
        start_defense(own, decision.reason);
        break;
    }
}

void Otopilot::reassess(Aircraft& own)
{
    const Decision<ReassessChoice> decision = decide::reassess(own, mission_);
    switch (decision.choice)
    {
    case ReassessChoice::Abort:
        record(own, "REASSESS → ABORT", decision.reason);
        go_to(OtopilotState::Abort);
        break;
    case ReassessChoice::Chase:
        record(own, "REASSESS → CHASE", decision.reason);
        go_to(OtopilotState::Chase);
        break;
    case ReassessChoice::Cold:
        record(own, "REASSESS → COLD", decision.reason);
        go_to(OtopilotState::Cold);
        break;
    case ReassessChoice::Out:
        record(own, "REASSESS → OUT", decision.reason);
        go_to(OtopilotState::Out);
        break;
    case ReassessChoice::Recommit:
        record(own, "REASSESS → RECOMMIT", decision.reason);
        go_to(OtopilotState::Recommit);
        break;
    }
}

// ========================================================================================================
// SAVUNMA
// ========================================================================================================

void Otopilot::start_defense(Aircraft& own, const char* text)
{
    const Decision<Defense> decision = decide::defense(own, mission_);
    defense_ = decision.choice;
    record(own, (std::string("DEFEND: ") + text).c_str(), decision.reason);
    go_to(OtopilotState::Defend);
}

void Otopilot::update_defense(Aircraft& own)
{
    const Decision<Defense> decision = decide::defense(own, mission_);
    if (static_cast<int>(decision.choice) > static_cast<int>(defense_))
    {
        defense_ = decision.choice;
        record(own, "DEFEND: acilleşti", decision.reason);
    }
}

Maneuver& Otopilot::defense_maneuver()
{
    switch (defense_)
    {
    case Defense::Beam: return maneuvers_.beam;
    case Defense::Drag: return maneuvers_.drag;
    case Defense::Notch: return maneuvers_.notch;
    }
    return maneuvers_.drag;   // erişilmez; -Wswitch her değer için case ister
}

Maneuver& Otopilot::support_maneuver(const Aircraft& own)
{
    if (condition::enemy_cold(own))
    {
        return chase_maneuver();
    }
    return maneuvers_.crank;
}

Maneuver& Otopilot::chase_maneuver()
{
    maneuvers_.pursuit.set(CHASE_MODE, throttle::MAX_THRUST);
    return maneuvers_.pursuit;
}

// ========================================================================================================
// ORTAK
// ========================================================================================================

void Otopilot::go_to(OtopilotState next)
{
    state_ = next;
}

void Otopilot::record(Aircraft& own, const char* text, const char* reason) const
{
    own.decision = std::string(text) + " — " + reason + " (" + goal_name(mission_.goal) + " / " + risk_name(mission_.risk) + ")";
}
