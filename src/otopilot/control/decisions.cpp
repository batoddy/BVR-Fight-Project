#include "control/decisions.hpp"

#include <optional>

#include "model/missile.hpp"

#include "control/conditions.hpp"

// ========================================================================================================
// K1  —  atış ne zaman (her Commit'te; Recommit'ten sonra da)
// ========================================================================================================

// Menzil sırası genelde TR > DOR > MAR. NEZ'e hiçbir tutumda girilmez.
// Püskürt: TR'de at; füze pitbull'u DOR dışında olur, düşmanı erken döndürür. TR çözümü yoksa DOR'da.
// İmha: her tutumda DOR'da at, isabet için daha yakın. Riskli ikinci füzeyi MAR'da atar (K3).
Decision<FirstShot> decide::first_shot(const Aircraft& own, const Mission& mission)
{
    if (own.weapons.missiles_left() == 0)
    {
        return {FirstShot::Wait, "füze yok"};
    }
    if (mission.goal == MissionGoal::Repel && condition::enemy_in_my_tr(own))
    {
        return {FirstShot::Fire, "TR'ye girildi"};
    }
    if (condition::in_dor(own))
    {
        return {FirstShot::Fire, "DOR'a girildi"};
    }
    return {FirstShot::Wait, "atış eşiği gelmedi"};
}

// ========================================================================================================
// K3  —  füze havada: destek ne zaman biter
// ========================================================================================================

// İmha / Riskli: MAR'a kadar destek, MAR'da ikinci füzeyi atıp çık. Diğerleri: DOR'da çık.
Decision<SupportLimit> decide::support_limit(const Aircraft& own, const Mission& mission)
{
    if (mission.goal == MissionGoal::Destroy && mission.risk == RiskLevel::Aggressive)
    {
        if (condition::in_mar(own))
        {
            return {SupportLimit::FireAndEscape, "MAR'a girildi: ikinci atış ve çıkış"};
        }
        return {SupportLimit::Continue, "MAR'a kadar destek"};
    }
    if (condition::in_dor(own))
    {
        return {SupportLimit::Escape, "DOR'a girildi"};
    }
    return {SupportLimit::Continue, "destek sürüyor"};
}

// ========================================================================================================
// K4  —  bana füze geliyor
// ========================================================================================================

namespace
{
    constexpr double DEFENSE_MARGIN_S = 10.0;   // s, Out dönüşüne eklenen pay: füzeye bu kadar kala savunma başlar

    bool counter_fire_allowed(const Aircraft& own, RiskLevel risk)   // karşı atış: havada füzem yoksa, riske göre
    {
        if (!own.weapons.active_missiles().empty() || own.weapons.missiles_left() == 0)
        {
            return false;
        }
        switch (risk)
        {
        case RiskLevel::Safe: return false;                                  // doğrudan savunma
        case RiskLevel::Balanced: return condition::enemy_in_my_tr(own);
        case RiskLevel::Aggressive: return condition::enemy_in_my_wez(own);
        }
        return false;
    }

    bool supporting(const Aircraft& own)   // havada pitbull olmamış füzem var
    {
        const Missile* missile = own.weapons.active_missile();
        return missile != nullptr && !missile->target.is_pitbull;
    }
}

// Füzeye kalan süre (yetişme süresi) Out dönüş süresiyle karşılaştırılır:
//   ≤ dönüş            : Notch, kaçmaya vakit yok
//   ≤ dönüş + pay      : son an; Güvenli/Dengeli Out + Drag, Riskli Notch
//   daha uzun          : Güvenli/Dengeli Beam (füzeyi erit), Riskli Notch
// Yetişme süresi yoksa (füze şu an yetişemiyor) Beam: savunmada kalınır, tehdit bitince K5.
Decision<ThreatResponse> decide::defense(const Aircraft& own, const Mission& mission)
{
    const bool aggressive = mission.risk == RiskLevel::Aggressive;
    const std::optional<double> time_left_s = own.picture.threat ? own.picture.threat->target.catch_time_left_s : std::nullopt;
    const double out_turn_s = own.picture.out.turn.time_s;

    if (time_left_s && *time_left_s <= out_turn_s)
    {
        return {ThreatResponse::Notch, "füzeye kalan süre Out dönüşünden kısa"};
    }
    if (aggressive)
    {
        return {ThreatResponse::Notch, "Riskli: Notch"};
    }
    if (time_left_s && *time_left_s <= out_turn_s + DEFENSE_MARGIN_S)
    {
        return {ThreatResponse::Drag, "füze yakın: Out + Drag"};
    }
    return {ThreatResponse::Beam, "füze uzak: Beam"};
}

// Karşı atış: Güvenli yok, Dengeli düşman TR'mdeyse, Riskli WEZ'imdeyse; havada füzem yoksa.
// Füzem havada ve pitbull değilse son ana (dönüş + pay) kadar destek sürer; sonra savunma.
Decision<ThreatResponse> decide::threat_response(const Aircraft& own, const Mission& mission)
{
    const Missile* threat = own.picture.threat;
    if (threat == nullptr)
    {
        return {ThreatResponse::Continue, "tehdit yok"};
    }
    if (!threat->target.catch_time_left_s)
    {
        return {ThreatResponse::Continue, "füze yetişemiyor"};
    }
    if (counter_fire_allowed(own, mission.risk))
    {
        return {ThreatResponse::CounterFire, "karşı atış şartı var"};
    }
    const bool last_moment = *threat->target.catch_time_left_s <= own.picture.out.turn.time_s + DEFENSE_MARGIN_S;
    if (supporting(own) && !last_moment)
    {
        return {ThreatResponse::Continue, "füzemi destekliyorum"};
    }
    return defense(own, mission);
}

// ========================================================================================================
// K5  —  savunma bitti
// ========================================================================================================

Decision<AfterEscape> decide::after_escape(const Aircraft& own, const Mission& mission)
{
    (void)mission;   // örnek: Püskürt + düşman cold → Depart; Güvenli yalnız füze ≥ 2 ise Recommit
    if (own.weapons.missiles_left() > 0)
    {
        return {AfterEscape::Recommit, "füze var"};
    }
    return {AfterEscape::Depart, "füze bitti"};
}

// ========================================================================================================
// İSKELET  —  K2; gövde şimdiki akışın davranışını döndürür, örnekler yorumda
// ========================================================================================================

Decision<AfterShot> decide::after_shot(const Aircraft& own, const Mission& mission)
{
    (void)own;
    (void)mission;
    // Püskürt → Leave (L&L: at ve Out).
    // İmha → destek, riske göre: Güvenli Crank, Dengeli Crank, Riskli Commit (pitbull'a kadar burun düşmanda).
    return {AfterShot::Crank, "destek: Crank"};
}

