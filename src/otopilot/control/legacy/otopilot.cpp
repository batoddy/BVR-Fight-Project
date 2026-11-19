#include "control/legacy/otopilot.hpp"

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
    own.state = state_name(state_);   // kayıt için: step sonundaki durum
}

// ========================================================================================================
// FAZLAR
// ========================================================================================================

void Otopilot::step_commit(Aircraft& own)
{
    maneuvers_.fly(own, maneuvers_.commit);
    const Decision<CommitChoice> decision = decide::commit(own, mission_);
    switch (decision.choice)
    {
    case CommitChoice::Continue:
        break;
    case CommitChoice::Fire:
        go_to(own, OtopilotState::Shot, "COMMIT: ateş", decision.reason);
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
    if (own.weapons.fire() || own.weapons.missiles_left() == 0)
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
        go_to(own, OtopilotState::Shot, "SUPPORT: 2. atış", decision.reason);
        break;
    case SupportChoice::Out:
        go_to(own, OtopilotState::Out, "SUPPORT → OUT", decision.reason);
        break;
    case SupportChoice::Abort:
        go_to(own, OtopilotState::Abort, "SUPPORT → ABORT (L&L)", decision.reason);
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
        go_to(own, OtopilotState::Support, "TRADE → SUPPORT", decision.reason);
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
        go_to(own, OtopilotState::Cold, "OUT → COLD", "düşman arkada");
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
        go_to(own, OtopilotState::Recommit, "COLD → RECOMMIT", decision.reason);
        break;
    case ColdChoice::Chase:
        go_to(own, OtopilotState::Chase, "COLD → CHASE", decision.reason);
        break;
    }
}

void Otopilot::step_recommit(Aircraft& own)
{
    maneuvers_.fly(own, maneuvers_.recommit);
    if (condition::in_dor(own))
    {
        go_to(own, OtopilotState::Out, "RECOMMIT → OUT", "dönerken DOR'a girildi");
        return;
    }
    if (maneuvers_.recommit.status(own) == ManeuverStatus::Done)
    {
        go_to(own, OtopilotState::Commit, "RECOMMIT → COMMIT", "atış açısındayım");
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
        go_to(own, OtopilotState::Shot, "CHASE: ateş", decision.reason);
        break;
    case ChaseChoice::Commit:
        go_to(own, OtopilotState::Commit, "CHASE → COMMIT", decision.reason);
        break;
    case ChaseChoice::Out:
        go_to(own, OtopilotState::Out, "CHASE → OUT", decision.reason);
        break;
    case ChaseChoice::GiveUp:
        gave_up_chase_ = true;
        go_to(own, OtopilotState::Cold, "CHASE → COLD", decision.reason);
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
        go_to(own, OtopilotState::Abort, "ABORT", decision.reason);
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
        go_to(own, OtopilotState::Shot, "TEHDİT: karşı atış", decision.reason);
        break;
    case ThreatResponse::Trade:
        go_to(own, OtopilotState::Trade, "TEHDİT → TRADE", decision.reason);
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
        go_to(own, OtopilotState::Abort, "REASSESS → ABORT", decision.reason);
        break;
    case ReassessChoice::Chase:
        go_to(own, OtopilotState::Chase, "REASSESS → CHASE", decision.reason);
        break;
    case ReassessChoice::Cold:
        go_to(own, OtopilotState::Cold, "REASSESS → COLD", decision.reason);
        break;
    case ReassessChoice::Out:
        go_to(own, OtopilotState::Out, "REASSESS → OUT", decision.reason);
        break;
    case ReassessChoice::Recommit:
        go_to(own, OtopilotState::Recommit, "REASSESS → RECOMMIT", decision.reason);
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
    go_to(own, OtopilotState::Defend, (std::string("DEFEND: ") + text).c_str(), decision.reason);
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
    maneuvers_.pursuit.set(CHASE_MODE, throttle::MRT);
    return maneuvers_.pursuit;
}

// ========================================================================================================
// ORTAK
// ========================================================================================================

void Otopilot::go_to(OtopilotState next)
{
    state_ = next;
}

void Otopilot::go_to(Aircraft& own, OtopilotState next, const char* text, const char* reason)
{
    record(own, text, reason);
    state_ = next;
}

void Otopilot::record(Aircraft& own, const char* text, const char* reason) const
{
    own.decision = std::string(text) + " — " + reason + " (" + goal_name(mission_.goal) + " / " + risk_name(mission_.risk) + ")";
}
