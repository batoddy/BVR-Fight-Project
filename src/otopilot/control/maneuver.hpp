#pragma once

#include "model/aircraft.hpp"
#include "model/flight_command.hpp"
#include "model/phase.hpp"

// =============================================================================
// ORTAK  —  bütün manevraların arayüzü ve uçurma yardımcısı
// =============================================================================

// Manevranın durumu.
enum class ManeuverStatus
{
    Running,   // sürüyor
    Done,      // kendi hedefine ulaştı (yalnız kendi içinde biten manevralar)
};

// Bütün manevraların ortak arayüzü. Manevra yalnız "bu durumda nasıl uçarım" sorusunu cevaplar;
// hangisinin çalışacağını otopilot seçer.
class Maneuver
{
public:
    virtual ~Maneuver() = default;

    /// Bu manevranın fazı (görüntüleyicide ve CSV'de görünen ad).
    virtual Phase phase() const = 0;

    /// Manevraya geçilince bir kez: girişe bağlı hedefleri kilitler.
    virtual void enter(const Aircraft& own) { (void)own; }

    /// Her step: own.command'dan (Python'un son uyguladığı) başlayıp yeni komutu döner.
    virtual FlightCommand update(const Aircraft& own) = 0;

    /// Kendi içinde biten manevralar (Out, Recommit) hedefe varınca Done döner;
    /// sürekli manevralar (Hold, Commit, Crank, Beam) hep Running döner, onları otopilot bitirir.
    virtual ManeuverStatus status(const Aircraft& own) const
    {
        (void)own;
        return ManeuverStatus::Running;
    }
};

// =============================================================================
// HOLD  —  şu anki yön ve irtifayla, aynı gazla devam
// =============================================================================

// NOT: her step ölçülen yön ve irtifayı hedef yapar; irtifa kayarsa düzeltilmez. Girişte kilitlemek ayrı bir karar.
class Hold : public Maneuver
{
public:
    Phase phase() const override { return Phase::Hold; }

    /// Yön ve irtifa = şu anki ölçüm, gaz aynı, g sınırı yok.
    FlightCommand update(const Aircraft& own) override;
};

// =============================================================================
// FIRE  —  atış anı; atış emrini otopilot verir (own.weapons.fire)
// =============================================================================

class Fire : public Maneuver
{
public:
    Phase phase() const override { return Phase::Fire; }

    /// Komut değişmez.
    FlightCommand update(const Aircraft& own) override;
};

// =============================================================================
// OUT  —  düşmana kuyruk dön, alçal, tam art yakıcı; düşman arkada kalınca biter
// =============================================================================

class Out : public Maneuver
{
public:
    Phase phase() const override { return Phase::Out; }

    /// Hedef irtifayı kilitler: DESCEND_FT alçal, FLOOR_FT'nin altına inme.
    void enter(const Aircraft& own) override;

    /// Yön = bearing − 180, irtifa = kilitli hedef, tam gaz, out g'si.
    FlightCommand update(const Aircraft& own) override;

    /// Düşman arkamda kaldıysa (|ATA| > DONE_ATA_DEG) Done.
    ManeuverStatus status(const Aircraft& own) const override;

private:
    static constexpr double DESCEND_FT = 3300.0;    // ft, out'ta ne kadar alçalınacak
    static constexpr double FLOOR_FT = 10000.0;     // ft, out bunun altına indirmez
    static constexpr double THROTTLE = 1.0;         // tam art yakıcı
    static constexpr double DONE_ATA_DEG = 177.0;   // derece, |ATA| bunu geçince düşman arkada: bitti

    double target_altitude_ft_ = 0;                 // ft, enter()'da bir kez hesaplanır, out boyunca sabit
};

// =============================================================================
// COMMIT  —  düşmana doğru düz uç (saf takip)
// =============================================================================

class Commit : public Maneuver
{
public:
    Phase phase() const override { return Phase::Commit; }

    /// Yön = düşmanın bearing'i; irtifa ve gaz aynı.
    FlightCommand update(const Aircraft& own) override;
};


// Manevra yönü, havacılık terimiyle: uçağın döndüğü taraf (Crank ve Recommit).
enum class ManeuverSide
{
    Right,   // uçak sağa döner. Crank: düşman burnun SOLUNDA kalır (ATA −). Recommit: SAĞINDA kalır (ATA +).
    Left,    // uçak sola döner. Crank: düşman burnun SAĞINDA kalır (ATA +). Recommit: SOLUNDA kalır (ATA −).
};

// =============================================================================
// RECOMMIT  —  düşmana dönüp atış açısına gel
// =============================================================================

/// Recommit'e girerken hangi taraf, kısa dönüşle: düşman sağımdaysa (ATA ≥ 0) Right, solumdaysa Left.
ManeuverSide recommit_side_for(const Aircraft& own);

// Taraf girişte seçilir ve recommit boyunca sabit kalır.
class Recommit : public Maneuver
{
public:
    Phase phase() const override { return Phase::Recommit; }

    /// Tarafı seçer (recommit_side_for).
    void enter(const Aircraft& own) override;

    /// Yön = bearing ∓ recommit_fire_ata_deg (Right −, Left +) + LOS_RATE_KP × HEADING_LAG_S × LOS hızı; irtifa ve gaz aynı.
    FlightCommand update(const Aircraft& own) override;

    /// Atış açısına geldiyse (|ATA| ≤ recommit_fire_ata_deg + DONE_TOLERANCE_DEG) Done.
    ManeuverStatus status(const Aircraft& own) const override;

    /// Girişte seçilen taraf.
    ManeuverSide side() const { return side_; }

private:

    static constexpr double TURN_INSIDE_DEG = 10.0;   // derece, atış açısına varış payı; uçak hedefin 2–3° gerisinde oturur
    static constexpr double DONE_TOLERANCE_DEG = 1.0;   // derece, atış açısına varış payı; uçak hedefin 2–3° gerisinde oturur
    static constexpr double HEADING_LAG_S = 2.0;        // s, yön kontrolünün gecikmesi (flight_control.py TAU_HEAD)
    static constexpr double LOS_RATE_KP = 1.3;          // birimsiz, gecikme payının çarpanı

    ManeuverSide side_ = ManeuverSide::Left;   // enter()'da seçilir
};

// =============================================================================
// CRANK —  düşmanı radar gimbal sınırının biraz içinde tutarak yanal kaçış
// =============================================================================

/// Crank'e girerken hangi taraf: TA ≥ 0 (düşmanın burnunun sağındayım) ise Left, değilse Right.
/// Seçimi düşmanın nereye gittiği belirler; düşmanın benim burnuma göre konumu değil.
ManeuverSide crank_side_for(const Aircraft& own);

// Menzil kapanmasını yavaşlatır.
// Taraf girişte seçilir ve crank boyunca sabit kalır.
class Crank : public Maneuver
{
public:
    Phase phase() const override { return Phase::Crank; }

    /// Tarafı seçer (crank_side_for).
    void enter(const Aircraft& own) override;

    /// Yön = bearing ± (gimbal − OFFSET_DEG) (Right +, Left −) + LOS_RATE_KP × HEADING_LAG_S × LOS hızı; irtifa ve gaz aynı.
    FlightCommand update(const Aircraft& own) override;

    /// Düşman radarın görüş konisinde mi (|ATA| < gimbal sınırı).
    bool in_radar_sight(const Aircraft& own) const;

    /// Girişte seçilen taraf.
    ManeuverSide side() const { return side_; }

private:
    static constexpr double OFFSET_DEG = 5.0;           // derece, gimbal sınırından içeri pay
    static constexpr double HEADING_LAG_S = 2.0;        // s, yön kontrolünün gecikmesi (flight_control.py TAU_HEAD)
    static constexpr double LOS_RATE_KP = 1.3;          // birimsiz, gecikme payının çarpanı

    ManeuverSide side_ = ManeuverSide::Left;   // enter()'da seçilir
};

// =============================================================================
// BEAM  —  düşman uçağa dik uç (3/9 hattı)
// =============================================================================

// Taraf burnuma göre seçilir: düşman sağımdaysa sola, solumdaysa sağa; dönüş hep 90°'den kısa.
// Füzeye göre olan notch ayrı bir manevra olacak.
class Beam : public Maneuver
{
public:
    Phase phase() const override { return Phase::Beam; }

    /// Yön = bearing − 90 (düşman sağımda) ya da bearing + 90 (solumda); irtifa ve gaz aynı.
    FlightCommand update(const Aircraft& own) override;
};

// =============================================================================
// MANEUVERS  —  bir otopilotun manevra nesneleri
// =============================================================================

// Her otopilot kendi manevralarını ve son uçurduğu manevrayı tutar.
// Yeni manevra: sınıfını yukarıya kendi banner'ıyla yaz, buraya bir satır ekle.
class Maneuvers
{
public:
    /// Manevra değiştiyse enter()'ı bir kez çağırır; ardından fazı ve uçuş komutunu yazar.
    void fly(Aircraft& own, Maneuver& maneuver);

    Hold hold;     // rotayı koru
    Fire fire;     // atış anı
    Out out;       // kaç
    Commit commit; // düşmana doğru düz uç
    Recommit recommit;   // taraf girişte seçilir
    Crank crank;   // taraf girişte seçilir
    Beam beam;     // düşmana dik uç

private:
    Maneuver* active_ = nullptr;   // son uçurulan manevra; ilk fly() da giriş sayılır
};
