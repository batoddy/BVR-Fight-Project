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

    /// Kendi içinde biten manevralar (Out) hedefe varınca Done döner;
    /// sürekli manevralar (Hold, In, Crank, Beam) hep Running döner, onları otopilot bitirir.
    virtual ManeuverStatus status(const Aircraft& own) const
    {
        (void)own;
        return ManeuverStatus::Running;
    }
};

/// Manevrayı bu step için uçur: own.phase'e manevranın fazını, own.command'a ürettiği komutu yazar.
void fly(Aircraft& own, Maneuver& maneuver);

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
// IN  —  düşmana dön (saf takip)
// =============================================================================

class In : public Maneuver
{
public:
    Phase phase() const override { return Phase::In; }

    /// Yön = düşmanın bearing'i; irtifa ve gaz aynı.
    FlightCommand update(const Aircraft& own) override;
};

// =============================================================================
// CRANK  —  düşmanı radar gimbal sınırının biraz içinde tutarak yanal kaçış
// =============================================================================

// Crank yönü, havacılık terimiyle: uçağın döndüğü taraf.
enum class CrankSide
{
    Left,    // uçak sola döner, düşman burnun SAĞINDA kalır (hedef ATA +)
    Right,   // uçak sağa döner, düşman burnun SOLUNDA kalır (hedef ATA −)
};

/// Crank'e girerken hangi taraf: düşman burnumun sağındaysa (ATA ≥ 0) Left, solundaysa Right.
/// Böylece düşman zaten bulunduğu tarafta kalır, uçak gereksiz yere ters tarafa dönmez.
CrankSide crank_side_for(const Aircraft& own);

// Menzil kapanmasını yavaşlatır. Yön P kontrolüyle tutulur: düzeltme = kp × (ATA − hedef ATA).
// Taraf girişte seçilir ve crank boyunca sabit kalır.
class Crank : public Maneuver
{
public:
    Phase phase() const override { return Phase::Crank; }

    /// Tarafı seçer (crank_side_for).
    void enter(const Aircraft& own) override;

    /// Yön = şu anki yön + P düzeltmesi (en fazla ±MAX_CORRECTION_DEG); irtifa ve gaz aynı.
    FlightCommand update(const Aircraft& own) override;

    /// Düşman radarın görüş konisinde mi (|ATA| < gimbal sınırı).
    bool in_radar_sight(const Aircraft& own) const;

    /// Girişte seçilen taraf.
    CrankSide side() const { return side_; }

private:
    static constexpr double OFFSET_DEG = 5.0;              // derece, gimbal sınırından içeri pay
    static constexpr double KP = 1.0;                      // birimsiz: hatanın kaç katı dön
    static constexpr double MAX_CORRECTION_DEG = 180.0;    // derece, tek seferde istenen en büyük dönüş

    /// derece ±180, düşmanın burnuma göre tutulacağı açı: Left'te +, Right'ta −.
    double target_ata_deg(const Aircraft& own) const;

    CrankSide side_ = CrankSide::Left;   // enter()'da seçilir
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

// Her biri bir kez oluşur ve kendi durumunu korur.
// Yeni manevra: sınıfını yukarıya kendi banner'ıyla yaz, buraya bir satır ekle.
struct Maneuvers
{
    Hold hold;     // rotayı koru
    Fire fire;     // atış anı
    Out out;       // kaç
    In in;         // düşmana dön
    Crank crank;   // taraf girişte seçilir
    Beam beam;     // düşmana dik uç
};
