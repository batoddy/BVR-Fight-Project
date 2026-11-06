#pragma once

#include "control/conditions.hpp"
#include "control/maneuver.hpp"
#include "model/aircraft.hpp"
#include "model/tactic_id.hpp"

// =============================================================================
// ORTAK  —  bütün taktiklerin arayüzü
// =============================================================================

enum class TacticStatus
{
    Running,   // sürüyor
    Done,      // kendi sonuna ulaştı
};

// Taktik: manevraların kurallı sırası. Her step o adımın manevrasını uçurur, sonra geçiş kurallarına bakar.
// Adımlar her taktiğin kendi Step enum'u; geçişler update() içindeki switch'te, kurallar isimli fonksiyonlarda.
// Atış manevra değil: atış adımı yalnız own.weapons.fire emrini verir, önceki manevranın komutu sürer.
// Gaz manevranın içinde (throttle::); taktik gaz yazmaz.
// Hangi taktiğin çalışacağını otopilot (ileride karar ağacı) seçer: Tactics::start.
class Tactic
{
public:
    virtual ~Tactic() = default;

    virtual TacticId id() const = 0;
    void start(const Aircraft& own)                                  // baştan başlat: Running, sonra enter(); Tactics::start çağırır
    {
        status_ = TacticStatus::Running;
        enter(own);
    }
    virtual void update(Aircraft& own, Maneuvers& maneuvers) = 0;    // her step: adımın manevrasını uçur, geçişlere bak
    virtual void exit(Aircraft& own) { (void)own; }                  // bitmeden başka taktiğe geçilirken bir kez: kalıcı komutları temizle
    TacticStatus status() const { return status_; }

protected:
    virtual void enter(const Aircraft& own) { (void)own; }           // başlarken bir kez: ilk adımı seç, girişe bağlı değerleri kilitle
    void finish() { status_ = TacticStatus::Done; }                  // Tactics sıradakini seçene kadar update sürebilir

private:
    TacticStatus status_ = TacticStatus::Running;
};

namespace tactic
{

// =============================================================================
// COMMIT  —  düşmana uç; bitmez, dışarıdan kesilir
// =============================================================================

class Commit : public Tactic
{
public:
    TacticId id() const override { return TacticId::Commit; }
    void update(Aircraft& own, Maneuvers& maneuvers) override;   // Commit manevrasını uçar
};

// =============================================================================
// SKATE AİLESİ  —  Long Skate, Skate, Short Skate: aynı adımlar, farklı kurallar
// =============================================================================

// Ailenin geçiş kuralları. Yalnız bu aileye özel; ortak olanlar condition:: içinde.
namespace skate_rule
{
    bool shoot_now(const Aircraft& own);         // hemen at: recommit burnu atış açısına getirdi
    bool long_shot_range(const Aircraft& own);   // İSKELET: Long Skate atış menzili; şimdilik hep false
    bool long_out_range(const Aircraft& own);    // İSKELET: Long Skate out menzili; şimdilik hep false
}

struct SkateParams   // Skate ailesinin bir üyesi
{
    TacticId id;
    bool (*shot_ready)(const Aircraft& own);   // Approach → Shoot
    bool (*out_ready)(const Aircraft& own);    // Support → Out
};

// Skate: düşman WEZ'ime girince at. TODO: doktrin "TR'den önce at" der; TR ile sınırlamak konuşulacak.
inline constexpr SkateParams LONG_SKATE{TacticId::LongSkate, skate_rule::long_shot_range, skate_rule::long_out_range};
inline constexpr SkateParams SKATE{TacticId::Skate, condition::enemy_in_my_wez, condition::in_dor};
inline constexpr SkateParams SHORT_SKATE{TacticId::ShortSkate, skate_rule::shoot_now, condition::in_mar};

// Approach → Shoot → Support → Out → bitti. Approach'ta MAR'a girersem atmadan Out.
// Skate ve Short Skate otopilot_fighter'daki eski akışın aynısı; Long Skate'in kuralları iskelet.
class Skate : public Tactic
{
public:
    enum class Step
    {
        Approach,   // Commit: atış kuralını bekle
        Shoot,      // atış emri; kabul edilince Support
        Support,    // Crank: out kuralını bekle
        Out,        // Out: düşman arkada kalınca biter
    };

    explicit Skate(const SkateParams& params) : params_(params) {}

    TacticId id() const override { return params_.id; }
    void update(Aircraft& own, Maneuvers& maneuvers) override;
    void exit(Aircraft& own) override;           // İSKELET: out yarıda kesilirse g sınırını bırak
    Step step() const { return step_; }          // şu anki adım

protected:
    void enter(const Aircraft& own) override;

private:
    void go_to(Step next) { step_ = next; }

    SkateParams params_;
    Step step_ = Step::Approach;
};

// =============================================================================
// BANZAI  —  atış, beam, kilit varsa out, yoksa takip (İSKELET)
// =============================================================================

// BMS §18.5: FOX3 DR'dan önce; düşmana göre notch (bizde Beam) ~15 s; kilit varsa abort, yoksa takip.
class Banzai : public Tactic
{
public:
    enum class Step
    {
        Approach,   // Commit: DR'dan önce atış
        Shoot,      // atış emri
        Beam,       // Beam: BEAM_TIME_S bekle
        Out,        // Out: kilitliysem
        Pursue,     // Commit: kilit yoksa düşmana dön
    };

    TacticId id() const override { return TacticId::Banzai; }
    void update(Aircraft& own, Maneuvers& maneuvers) override;
    Step step() const { return step_; }

protected:
    void enter(const Aircraft& own) override;

private:
    static constexpr double BEAM_TIME_S = 15.0;   // s, profile.dr_notch_time_s ile aynı

    void go_to(Step next) { step_ = next; }

    Step step_ = Step::Approach;
};

// =============================================================================
// RECOMMIT  —  düşmana dönüp atış açısına gel; atış Short Skate'in ilk adımı
// =============================================================================

class Recommit : public Tactic
{
public:
    TacticId id() const override { return TacticId::Recommit; }
    void update(Aircraft& own, Maneuvers& maneuvers) override;   // Recommit manevrası; atış açısına gelince bitti
};

// =============================================================================
// ABORT  —  out; düşman arkada kalınca biter
// =============================================================================

class Abort : public Tactic
{
public:
    TacticId id() const override { return TacticId::Abort; }
    void update(Aircraft& own, Maneuvers& maneuvers) override;   // Out manevrası; bitince bitti
};

// =============================================================================
// FLANK ATTACK  —  yan açı kazan, sonra atış (İSKELET)
// =============================================================================

class FlankAttack : public Tactic
{
public:
    enum class Step
    {
        Flank,     // Flank: düşmanın burnuna göre yan açıya geç
        Turn,      // Recommit: burnu düşmana çevir
        Shoot,     // atış emri
        Support,   // Crank
    };

    TacticId id() const override { return TacticId::FlankAttack; }
    void update(Aircraft& own, Maneuvers& maneuvers) override;
    Step step() const { return step_; }

protected:
    void enter(const Aircraft& own) override;

private:
    void go_to(Step next) { step_ = next; }

    Step step_ = Step::Flank;
};

// =============================================================================
// BEAM DEFENSE  —  düşmana dik uç (İSKELET)
// =============================================================================

class BeamDefense : public Tactic
{
public:
    TacticId id() const override { return TacticId::BeamDefense; }
    void update(Aircraft& own, Maneuvers& maneuvers) override;   // örnek: Beam; süre dolunca ya da spike kalkınca bitti
};

// =============================================================================
// NOTCH DEFENSE  —  füzeye dik uç (İSKELET)
// =============================================================================

class NotchDefense : public Tactic
{
public:
    TacticId id() const override { return TacticId::NotchDefense; }
    void update(Aircraft& own, Maneuvers& maneuvers) override;   // örnek: Notch; füze geçince ya da bitince bitti
};

// =============================================================================
// DRAG DEFENSE  —  füzeye sırt dön (İSKELET)
// =============================================================================

class DragDefense : public Tactic
{
public:
    TacticId id() const override { return TacticId::DragDefense; }
    void update(Aircraft& own, Maneuvers& maneuvers) override;   // örnek: Drag; füze bitince bitti
};

// =============================================================================
// GUIDE & EVADE  —  füzeyi destekle, tehdit yaklaşınca kaç (İSKELET)
// =============================================================================

// BVRGym'deki "guide + evade" dalı: füzem pitbull olana kadar destek, tehdit yakınsa kaçış.
class GuideEvade : public Tactic
{
public:
    enum class Step
    {
        Guide,   // Crank: füzemi destekle
        Evade,   // Notch ya da Drag: bana gelen füzeden kaç
    };

    TacticId id() const override { return TacticId::GuideEvade; }
    void update(Aircraft& own, Maneuvers& maneuvers) override;
    Step step() const { return step_; }

protected:
    void enter(const Aircraft& own) override;

private:
    void go_to(Step next) { step_ = next; }

    Step step_ = Step::Guide;
};

}   // namespace tactic

// =============================================================================
// TACTICS  —  bir otopilotun taktik nesneleri
// =============================================================================

// Her otopilot kendi taktiklerini ve çalışan taktiği tutar. Kullanım:
//   tactics_.start(own, tactics_.skate);                       // bir kez, seçimde
//   tactics_.update(own, maneuvers_);                          // her step
//   if (tactics_.skate.status() == TacticStatus::Done) { ... } // sıradaki taktiği seç
//   tactics_.skate.step() == tactic::Skate::Step::Support      // taktik içinde neredeyim
//   own.phase == Phase::Crank                                  // hangi manevrayı uçuyorum
// Yeni taktik: sınıfını yukarıya kendi banner'ıyla yaz, buraya bir satır ekle.
class Tactics
{
public:
    void start(Aircraft& own, Tactic& tactic);            // önceki başkaysa exit(), sonra bu taktik baştan; aynı taktik de baştan başlar
    void update(Aircraft& own, Maneuvers& maneuvers);     // çalışan taktiğin update()'i; yoksa bir şey yapmaz
    const Tactic* active() const { return active_; }      // çalışan taktik; hiç başlatılmadıysa nullptr

    tactic::Commit commit;                          // düşmana uç
    tactic::Skate long_skate{tactic::LONG_SKATE};   // İSKELET kurallar
    tactic::Skate skate{tactic::SKATE};             // DOR'da out
    tactic::Skate short_skate{tactic::SHORT_SKATE}; // MAR'da out
    tactic::Banzai banzai;                          // İSKELET
    tactic::Recommit recommit;                      // atış açısına dön
    tactic::Abort abort_out;                        // out (std::abort ile karışmasın diye abort_out)
    tactic::FlankAttack flank_attack;               // İSKELET
    tactic::BeamDefense beam_defense;               // İSKELET
    tactic::NotchDefense notch_defense;             // İSKELET
    tactic::DragDefense drag_defense;               // İSKELET
    tactic::GuideEvade guide_evade;                 // İSKELET

private:
    Tactic* active_ = nullptr;
};
