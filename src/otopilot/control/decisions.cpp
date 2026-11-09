#include "control/decisions.hpp"

#include <optional>

#include "control/conditions.hpp"
#include "model/missile.hpp"

namespace
{
    constexpr double BEAM_MARGIN_NM = 5.0;     // NM, kaçış payı bundan büyükse Beam (menzili koru), değilse Drag
    constexpr double DEFENSE_MARGIN_S = 10.0;  // s, out dönüşüne eklenen pay: savunmaya en geç bu kadar önce geçilir
    constexpr double TRADE_MARGIN_S = 5.0;     // s, TRADE'de B2 ve B1 karşılaştırmalarının payı
    constexpr double NOTCH_MARGIN_S = 3.0;     // s, notch dönüşüne eklenen pay: tehdide bu kadar kala notch

    bool counter_fire_allowed(const Aircraft& own, RiskLevel risk)   // havada füzem yokken karşı atış, riske göre
    {
        if (condition::has_live_missile(own) || own.weapons.missiles_left() == 0)
        {
            return false;
        }
        switch (risk)
        {
        case RiskLevel::Safe: return false;
        case RiskLevel::Balanced: return condition::enemy_in_my_tr(own);
        case RiskLevel::Aggressive: return condition::enemy_in_my_wez(own);
        }
        return false;
    }
}

// ========================================================================================================
// KESİNTİLER
// ========================================================================================================

Decision<AbortChoice> decide::abort(const Aircraft& own)
{
    if (condition::out_of_missiles(own))
    {
        return {AbortChoice::Abort, "füzem kalmadı"};
    }
    if (condition::in_mar(own))
    {
        return {AbortChoice::Abort, "MAR ihlali"};
    }
    return {AbortChoice::Continue, "devam"};
}

// Havada füzem yoksa riske göre karşı atış; füzem pitbull değilse TRADE; diğer durumda savunma.
Decision<ThreatResponse> decide::threat_response(const Aircraft& own, const Mission& mission)
{
    if (!condition::threat_alive(own))
    {
        return {ThreatResponse::Continue, "aktif tehdit yok"};
    }
    if (counter_fire_allowed(own, mission.risk))
    {
        return {ThreatResponse::CounterFire, "karşı atış şartı var"};
    }
    if (condition::supporting(own))
    {
        return {ThreatResponse::Trade, "füzem havada, pitbull değil"};
    }
    return {ThreatResponse::Defend, "tehdit geliyor"};
}

// ========================================================================================================
// COMMIT  —  ilk atış noktası: şimdilik sabit, TR (Pk ikinci aşamada)
// ========================================================================================================

Decision<CommitChoice> decide::commit(const Aircraft& own)
{
    if (condition::enemy_cold(own))
    {
        return {CommitChoice::Reassess, "düşman cold"};
    }
    if (own.weapons.missiles_left() == 0 || !condition::enemy_in_my_wez(own))
    {
        return {CommitChoice::Continue, "WEZ dışı"};
    }
    if (condition::enemy_in_my_tr(own))
    {
        return {CommitChoice::Fire, "TR'ye girildi"};
    }
    if (!own.weapons.tr_nm && condition::in_dor(own))
    {
        return {CommitChoice::Fire, "TR çözümü yok: DOR'a girildi"};
    }
    return {CommitChoice::Continue, "atış noktası gelmedi"};
}

// ========================================================================================================
// SUPPORT  —  füzem havada, tehdit yok
// ========================================================================================================

// İmha + Riskli MSR'de 2. füzeyi atar ve en yeni füzeyi pitbull'a kadar destekler.
// Diğerleri füzem pitbull olunca ya da DOR'da (pitbull olmasa da) çıkar.
Decision<SupportChoice> decide::support(const Aircraft& own, const Mission& mission)
{
    if (own.weapons.launch_pending())
    {
        return {SupportChoice::Continue, "füzem henüz uçmadı"};
    }
    const Missile* missile = own.weapons.active_missile();
    if (missile == nullptr)
    {
        return {SupportChoice::Reassess, "füzem bitti"};
    }

    const bool aggressive_destroy = mission.goal == MissionGoal::Destroy && mission.risk == RiskLevel::Aggressive;
    const bool second_shot_left = own.weapons.missiles_left() > 0 && own.weapons.active_missiles().size() < 2;
    SupportChoice leave = SupportChoice::Out;
    if (mission.leave_after_support)
    {
        leave = SupportChoice::Abort;
    }

    if (aggressive_destroy && second_shot_left && condition::enemy_in_my_msr(own))
    {
        return {SupportChoice::SecondShot, "Riskli imha: MSR'de 2. atış"};
    }
    if (missile->target.is_pitbull)
    {
        if (condition::enemy_cold(own))
        {
            return {SupportChoice::Reassess, "füzem pitbull, düşman kaçıyor"};
        }
        if (aggressive_destroy && second_shot_left)
        {
            return {SupportChoice::Continue, "Riskli imha: MSR'ye kadar devam"};
        }
        return {leave, "füzem pitbull"};
    }
    if (!aggressive_destroy && condition::in_dor(own))
    {
        return {leave, "DOR: destek bırakıldı"};
    }
    return {SupportChoice::Continue, "destek sürüyor"};
}

// ========================================================================================================
// TRADE  —  iki füze havada, benimki pitbull değil
// ========================================================================================================

// Crank'i en geç t_kritik'te bırakırım:  t_kritik = t_vur_tehdit − (out dönüşü + pay)
//   A  (her risk)    : t_pb_ben ≤ t_kritik                              füzem kritik andan önce pitbull
//   B2 (Dengeli+)    : t_kritik_düşman < t_pb_tehdit ve
//                      t_kritik_düşman + pay < t_kritik                düşman benden önce kırmak zorunda, füzesi ölür
//                      t_kritik_düşman = t_vur_ben − (düşmanın out dönüşü + pay)
//   B1 (Riskli)      : t_vur_ben + pay < t_pb_tehdit                    füzem tehdit pitbull olmadan vurur
// Tehdit zaten pitbull ise yalnız A. Hiçbiri tutmazsa da t_kritik'e kadar crank sürer: bu sırada düşman kırabilir.
Decision<TradeChoice> decide::trade(const Aircraft& own, const Mission& mission)
{
    if (!condition::threat_alive(own))
    {
        return {TradeChoice::Support, "tehdit bitti"};
    }
    if (own.weapons.launch_pending())
    {
        return {TradeChoice::Continue, "füzem henüz uçmadı"};
    }
    const Missile* missile = own.weapons.active_missile();
    if (missile == nullptr)
    {
        return {TradeChoice::Defend, "füzem bitti"};
    }
    if (missile->target.is_pitbull)
    {
        return {TradeChoice::Defend, "füzem pitbull"};
    }

    const MissileTargetInfo& threat = own.picture.threat->target;
    const double threat_hit_s = *threat.catch_time_left_s;
    const double crank_left_s = threat_hit_s - (own.picture.out.turn.time_s + DEFENSE_MARGIN_S);

    const std::optional<double> my_pitbull_s = missile->target.pitbull_time_left_s;
    if (my_pitbull_s && *my_pitbull_s <= crank_left_s)
    {
        return {TradeChoice::Continue, "A: füzem kritik andan önce pitbull"};
    }

    const std::optional<double> my_hit_s = missile->target.catch_time_left_s;
    const std::optional<double> threat_pitbull_s = threat.pitbull_time_left_s;
    const bool threat_can_die = !threat.is_pitbull && threat_pitbull_s && my_hit_s;   // atan kırılırsa tehdit güdümünü kaybeder

    if (threat_can_die && mission.risk != RiskLevel::Safe)
    {
        const double enemy_break_s = *my_hit_s - (own.picture.enemy_out_turn_s + DEFENSE_MARGIN_S);
        if (enemy_break_s < *threat_pitbull_s && enemy_break_s + TRADE_MARGIN_S < crank_left_s)
        {
            return {TradeChoice::Continue, "B2: düşman önce kırmak zorunda"};
        }
    }
    if (threat_can_die && mission.risk == RiskLevel::Aggressive && *my_hit_s + TRADE_MARGIN_S < *threat_pitbull_s)
    {
        return {TradeChoice::Continue, "B1: füzem tehdit pitbull olmadan vurur"};
    }
    if (crank_left_s > 0.0)
    {
        return {TradeChoice::Continue, "kritik ana kadar crank"};
    }
    return {TradeChoice::Defend, "kritik an geldi: destek bırakıldı"};
}

// ========================================================================================================
// DEFEND  —  kaçış payına göre savunma tipi
// ========================================================================================================

// Kaçış payı (picture.threat_escape_margin_nm): şimdi out + drag yapsam füze ölene kadar aramda kalan mesafe.
//   pay > BEAM_MARGIN_NM, Güvenli değil      : Beam, menzili koru
//   0 ≤ pay                                   : Drag, kaçarım
//   pay < 0, tehdide kalan ≤ notch dönüşü + pay : Notch, son çare
//   pay < 0                                   : Drag, füzenin enerjisini tüket
Decision<Defense> decide::defense(const Aircraft& own, const Mission& mission)
{
    const std::optional<double> margin_nm = own.picture.threat_escape_margin_nm;
    if (!margin_nm)
    {
        return {Defense::Drag, "tehdit yok"};
    }
    if (*margin_nm > BEAM_MARGIN_NM && mission.risk != RiskLevel::Safe)
    {
        return {Defense::Beam, "kaçış payı bol: Beam"};
    }
    if (*margin_nm >= 0.0)
    {
        return {Defense::Drag, "drag ile kaçarım"};
    }

    const std::optional<double> threat_hit_s = own.picture.threat->target.catch_time_left_s;
    if (threat_hit_s && *threat_hit_s <= own.picture.notch_turn_s + NOTCH_MARGIN_S)
    {
        return {Defense::Notch, "son an: Notch"};
    }
    return {Defense::Drag, "kaçamıyorum: enerjisini tüket"};
}

// ========================================================================================================
// REASSESS  —  taahhüt bitti: savunma, destek ya da kovalama
// ========================================================================================================

Decision<ReassessChoice> decide::reassess(const Aircraft& own, const Mission& mission)
{
    if (condition::out_of_missiles(own))
    {
        return {ReassessChoice::Abort, "füzem kalmadı"};
    }
    if (condition::enemy_cold(own))
    {
        if (mission.goal == MissionGoal::Destroy && own.weapons.missiles_left() > 0)
        {
            return {ReassessChoice::Chase, "düşman kaçıyor: kovala"};
        }
        return {ReassessChoice::Cold, "düşman kaçıyor: bekle"};
    }
    if (condition::my_aspect(own) == Aspect::Cold)
    {
        return {ReassessChoice::Cold, "cold'dayım: MRR'ye açıl"};
    }
    if (condition::in_dor(own))
    {
        return {ReassessChoice::Out, "DOR içinde"};
    }
    return {ReassessChoice::Recommit, "düşman hot, DOR dışı"};
}

// ========================================================================================================
// COLD  —  cold ops: MRR'ye açıl, recommit anını bekle
// ========================================================================================================

// Recommit kaçış payı (picture.threat_recommit_margin_nm): havada bana gelen füze varken dönsem yetişir mi.
Decision<ColdChoice> decide::cold(const Aircraft& own, const Mission& mission, bool gave_up_chase)
{
    if (condition::enemy_hot(own))
    {
        if (own.weapons.missiles_left() == 0)
        {
            return {ColdChoice::Run, "son füzem havada: sonucu bekle"};
        }
        if (!condition::beyond_mrr(own))
        {
            return {ColdChoice::Run, "MRR'ye açılıyorum"};
        }
        const std::optional<double> recommit_margin_nm = own.picture.threat_recommit_margin_nm;
        if (recommit_margin_nm && *recommit_margin_nm < 0.0)
        {
            return {ColdChoice::Run, "recommit yaparsam tehdit yetişir"};
        }
        return {ColdChoice::Recommit, "MRR dışı"};
    }
    if (condition::enemy_cold(own))
    {
        if (mission.goal == MissionGoal::Destroy && !gave_up_chase && own.weapons.missiles_left() > 0)
        {
            return {ColdChoice::Chase, "düşman kaçıyor: kovala"};
        }
        if (condition::beyond_mrr(own))
        {
            return {ColdChoice::Hold, "düşman kaçıyor: bekle"};
        }
        return {ColdChoice::Run, "MRR'ye açılıyorum"};
    }
    return {ColdChoice::Run, "düşman beam: bekle"};
}

// ========================================================================================================
// CHASE  —  İmha, düşman kaçıyor
// ========================================================================================================

Decision<ChaseChoice> decide::chase(const Aircraft& own)
{
    if (condition::enemy_hot(own))
    {
        if (condition::in_dor(own))
        {
            return {ChaseChoice::Out, "düşman döndü, DOR içinde"};
        }
        return {ChaseChoice::Commit, "düşman döndü"};
    }
    if (own.weapons.missiles_left() > 0 && !condition::has_live_missile(own) && condition::enemy_in_my_r_cold(own))
    {
        return {ChaseChoice::Fire, "kaçan düşman R_cold'umda"};
    }
    if (condition::my_aspect(own) == Aspect::Hot && own.picture.closure_kt <= 0.0)
    {
        return {ChaseChoice::GiveUp, "yetişemiyorum"};
    }
    return {ChaseChoice::Continue, "kovalıyorum"};
}
