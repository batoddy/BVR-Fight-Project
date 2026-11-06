#include "control/tactic.hpp"

// ========================================================================================================
// TACTICS
// ========================================================================================================

// NOT: yeni taktik öncekinin son manevrasıyla başlarsa (Skate'in Out'u → Abort'un Out'u) Maneuvers::fly
// enter()'ı yeniden çağırmaz. Gerekirse: maneuvers.restart().
void Tactics::start(Aircraft& own, Tactic& tactic)
{
    if (active_ != nullptr && active_ != &tactic)
    {
        active_->exit(own);   // yarıda kalan taktik kalıcı komutlarını temizler
    }
    tactic.start(own);
    active_ = &tactic;
}

void Tactics::update(Aircraft& own, Maneuvers& maneuvers)
{
    if (active_ == nullptr)
    {
        return;
    }
    active_->update(own, maneuvers);
    // own.tactic = active_->id();   // Aircraft'a alan, CSV'ye ve görüntüleyiciye sütun eklenince
}

namespace tactic
{

// ========================================================================================================
// COMMIT  —  düşmana uç; bitmez, dışarıdan kesilir
// ========================================================================================================

void Commit::update(Aircraft& own, Maneuvers& maneuvers)
{
    maneuvers.fly(own, maneuvers.commit);
    // if (condition::in_mar(own)) { finish(); }   // kendi içinde bitmek isterse örnek
}

// ========================================================================================================
// SKATE AİLESİ  —  Approach → Shoot → Support → Out → bitti
// ========================================================================================================

namespace skate_rule
{
    bool shoot_now(const Aircraft& own)
    {
        (void)own;
        return true;
    }

    bool long_shot_range(const Aircraft& own)
    {
        (void)own;
        // return own.picture.range_3d_nm < own.weapons.wez.max_nm * 0.9;   // örnek: WEZ'in dış kısmında at
        return false;
    }

    bool long_out_range(const Aircraft& own)
    {
        (void)own;
        // return own.picture.range_3d_nm < own.picture.dor_nm + 5.0;       // örnek: DOR'a bir pay kala çık
        return false;
    }
}

void Skate::enter(const Aircraft& own)
{
    (void)own;
    step_ = Step::Approach;
}

void Skate::update(Aircraft& own, Maneuvers& maneuvers)   // otopilot_fighter'ın ilk yarısı; kurallar params_'tan
{
    switch (step_)
    {
    case Step::Approach:
        maneuvers.fly(own, maneuvers.commit);
        if (condition::in_mar(own))
        {
            go_to(Step::Out);   // atmadan MAR'a girdim: kaç
        }
        else if (params_.shot_ready(own))
        {
            go_to(Step::Shoot);
        }
        break;

    case Step::Shoot:                                       // manevra uçmaz, Approach'un komutu sürer
        if (own.weapons.fire(own.profile.main_weapon))      // kabul edilmezse (önceki emir uçmadı) sonraki step tekrar
        {
            go_to(Step::Support);
        }
        break;

    case Step::Support:
        maneuvers.fly(own, maneuvers.crank);
        if (params_.out_ready(own))
        {
            go_to(Step::Out);
        }
        break;

    case Step::Out:
        maneuvers.fly(own, maneuvers.out);
        if (maneuvers.out.status(own) == ManeuverStatus::Done)
        {
            finish();
        }
        break;
    }
}

void Skate::exit(Aircraft& own)
{
    (void)own;
    // if (step_ == Step::Out) { own.command.clear_g_limit(); }   // örnek: out yarıda kesilirse g sınırını bırak
}

// ========================================================================================================
// BANZAI  —  atış, beam, kilit varsa out, yoksa takip (İSKELET)
// ========================================================================================================

void Banzai::enter(const Aircraft& own)
{
    (void)own;
    step_ = Step::Approach;
}

void Banzai::update(Aircraft& own, Maneuvers& maneuvers)
{
    (void)own;
    (void)maneuvers;

    // switch (step_)
    // {
    // case Step::Approach:
    //     maneuvers.fly(own, maneuvers.commit);
    //     if (own.picture.range_3d_nm < own.picture.dr_nm + 2.0) { go_to(Step::Shoot); }   // DR'dan önce
    //     break;
    //
    // case Step::Shoot:
    //     if (own.weapons.fire(own.profile.main_weapon)) { go_to(Step::Beam); }
    //     break;
    //
    // case Step::Beam:
    //     maneuvers.fly(own, maneuvers.beam);
    //     if (step_time_s(now_s) < BEAM_TIME_S) { break; }          // World::sim_time_s otopilota iletilince
    //     go_to(condition::is_spiked(own) ? Step::Out : Step::Pursue);   // spike: şimdilik füze geliyor mu
    //     break;
    //
    // case Step::Out:
    //     maneuvers.fly(own, maneuvers.out);
    //     if (maneuvers.out.status(own) == ManeuverStatus::Done) { finish(); }
    //     break;
    //
    // case Step::Pursue:
    //     maneuvers.fly(own, maneuvers.commit);
    //     break;
    // }
}

// ========================================================================================================
// RECOMMIT  —  düşmana dönüp atış açısına gel
// ========================================================================================================

void Recommit::update(Aircraft& own, Maneuvers& maneuvers)   // otopilot_fighter'ın Recommit durumu
{
    maneuvers.fly(own, maneuvers.recommit);
    if (maneuvers.recommit.status(own) == ManeuverStatus::Done)
    {
        finish();
    }
}

// ========================================================================================================
// ABORT  —  out; düşman arkada kalınca biter
// ========================================================================================================

void Abort::update(Aircraft& own, Maneuvers& maneuvers)   // otopilot_fighter'ın FinalEscape durumu
{
    maneuvers.fly(own, maneuvers.out);
    if (maneuvers.out.status(own) == ManeuverStatus::Done)
    {
        finish();
    }
}

// ========================================================================================================
// FLANK ATTACK  —  yan açı kazan, sonra atış (İSKELET)
// ========================================================================================================

void FlankAttack::enter(const Aircraft& own)
{
    (void)own;
    step_ = Step::Flank;
}

void FlankAttack::update(Aircraft& own, Maneuvers& maneuvers)
{
    (void)own;
    (void)maneuvers;

    // switch (step_)
    // {
    // case Step::Flank:
    //     maneuvers.fly(own, maneuvers.flank);
    //     if (maneuvers.flank.status(own) == ManeuverStatus::Done) { go_to(Step::Turn); }
    //     if (condition::in_dor(own)) { finish(); }   // yan açıyı kazanamadan çok yaklaştım
    //     break;
    //
    // case Step::Turn:
    //     maneuvers.fly(own, maneuvers.recommit);
    //     if (maneuvers.recommit.status(own) == ManeuverStatus::Done && condition::enemy_in_my_wez(own))
    //     {
    //         go_to(Step::Shoot);
    //     }
    //     break;
    //
    // case Step::Shoot:
    //     if (own.weapons.fire(own.profile.main_weapon)) { go_to(Step::Support); }
    //     break;
    //
    // case Step::Support:
    //     maneuvers.fly(own, maneuvers.crank);
    //     if (condition::in_dor(own)) { finish(); }   // sıradaki taktik (Abort, Recommit) dışarıdan seçilir
    //     break;
    // }
}

// ========================================================================================================
// BEAM DEFENSE  —  düşmana dik uç (İSKELET)
// ========================================================================================================

void BeamDefense::update(Aircraft& own, Maneuvers& maneuvers)
{
    (void)own;
    (void)maneuvers;

    // maneuvers.fly(own, maneuvers.beam);
    // if (!condition::is_spiked(own)) { finish(); }   // ya da süre dolunca
}

// ========================================================================================================
// NOTCH DEFENSE  —  füzeye dik uç (İSKELET)
// ========================================================================================================

void NotchDefense::update(Aircraft& own, Maneuvers& maneuvers)
{
    (void)own;
    (void)maneuvers;

    // maneuvers.fly(own, maneuvers.notch);
    // if (!condition::is_spiked(own)) { finish(); }                                                     // füze geçti ya da bitti
    // const Missile* threat = own.picture.threat;
    // if (threat != nullptr && threat->target.range_nm < 5.0) { maneuvers.fly(own, maneuvers.drag); }   // füze çok yakın: notch yerine drag
}

// ========================================================================================================
// DRAG DEFENSE  —  füzeye sırt dön (İSKELET)
// ========================================================================================================

void DragDefense::update(Aircraft& own, Maneuvers& maneuvers)
{
    (void)own;
    (void)maneuvers;

    // maneuvers.fly(own, maneuvers.drag);
    // if (!condition::is_spiked(own)) { finish(); }
}

// ========================================================================================================
// GUIDE & EVADE  —  füzeyi destekle, tehdit yaklaşınca kaç (İSKELET)
// ========================================================================================================

void GuideEvade::enter(const Aircraft& own)
{
    (void)own;
    step_ = Step::Guide;
}

void GuideEvade::update(Aircraft& own, Maneuvers& maneuvers)
{
    (void)own;
    (void)maneuvers;

    // switch (step_)
    // {
    // case Step::Guide:
    //     maneuvers.fly(own, maneuvers.crank);
    //     if (condition::own_missile_pitbull(own, 1)) { finish(); }   // destek bitti
    //     else if (condition::in_mar(own)) { go_to(Step::Evade); }   // tehdit çok yakın: desteği bırak
    //     break;
    //
    // case Step::Evade:
    //     maneuvers.fly(own, maneuvers.notch);
    //     if (!condition::is_spiked(own)) { finish(); }
    //     break;
    // }
}

}   // namespace tactic
