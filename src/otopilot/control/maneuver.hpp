#pragma once

#include "model/aircraft.hpp"
#include "model/flight_command.hpp"
#include "model/phase.hpp"

enum class ManeuverStatus
{
    Running,   // sürüyor
    Done,      // kendi hedefine ulaştı (yalnız kendi içinde biten manevralar)
};

// Bütün manevraların ortak arayüzü. Manevra yalnız "bu durumda nasıl uçarım" sorusunu cevaplar;
// hangisinin çalışacağını otopilot seçer.
//
// Yaşam döngüsü:
//   enter(own)   manevraya geçilince bir kez: girişe bağlı hedefleri kilitler
//   update(own)  her step: own.command'dan (Python'un son uyguladığı) başlayıp yeni komutu döner
//   status(own)  kendi içinde biten manevralar (Out) hedefe varınca Done döner;
//                sürekli manevralar (Hold, In, Crank, Beam) hep Running döner, onları otopilot bitirir
class Maneuver
{
public:
    virtual ~Maneuver() = default;

    virtual Phase phase() const = 0;

    virtual void enter(const Aircraft& own) { (void)own; }

    virtual FlightCommand update(const Aircraft& own) = 0;

    virtual ManeuverStatus status(const Aircraft& own) const
    {
        (void)own;
        return ManeuverStatus::Running;
    }
};

// Manevrayı bu step için uçur: own.phase'e manevranın fazını, own.command'a ürettiği komutu yazar.
void fly(Aircraft& own, Maneuver& maneuver);

// Şu anki yön ve irtifayla, aynı gazla devam.
// NOT: her step ölçülen yön ve irtifayı hedef yapar; irtifa kayarsa düzeltilmez. Girişte kilitlemek ayrı bir karar.
class Hold : public Maneuver
{
public:
    Phase phase() const override { return Phase::Hold; }
    FlightCommand update(const Aircraft& own) override;
};

// Atış anı: komut değişmez. Atış emrini otopilot verir (own.weapons.fire).
class Fire : public Maneuver
{
public:
    Phase phase() const override { return Phase::Fire; }
    FlightCommand update(const Aircraft& own) override;
};

// Düşmana kuyruk dön, alçal, tam art yakıcı. Düşman arkada kalınca biter.
class Out : public Maneuver
{
public:
    Phase phase() const override { return Phase::Out; }
    void enter(const Aircraft& own) override;
    FlightCommand update(const Aircraft& own) override;
    ManeuverStatus status(const Aircraft& own) const override;

private:
    static constexpr double DESCEND_FT = 3300.0;    // ft, out'ta ne kadar alçalınacak
    static constexpr double FLOOR_FT = 10000.0;     // ft, out bunun altına indirmez
    static constexpr double THROTTLE = 1.0;         // tam art yakıcı
    static constexpr double DONE_ATA_DEG = 177.0;   // derece, |ATA| bunu geçince düşman arkada: bitti

    double target_altitude_ft_ = 0;                 // ft, enter()'da bir kez hesaplanır, out boyunca sabit
};

// Düşmana dön (saf takip): yalnız yön değişir.
class In : public Maneuver
{
public:
    Phase phase() const override { return Phase::In; }
    FlightCommand update(const Aircraft& own) override;
};

// Crank yönü, havacılık terimiyle: uçağın döndüğü taraf.
enum class CrankSide
{
    Left,    // uçak sola döner, düşman burnun SAĞINDA kalır (hedef ATA +)
    Right,   // uçak sağa döner, düşman burnun SOLUNDA kalır (hedef ATA -)
};

// Crank'e girerken hangi taraf: düşman şu an burnumun sağındaysa (ATA ≥ 0) sola crank, solundaysa sağa.
// Böylece düşman zaten bulunduğu tarafta kalır, uçak gereksiz yere ters tarafa dönmez.
CrankSide crank_side_for(const Aircraft& own);

// Crank: düşmanı radar gimbal sınırının biraz içinde tutarak yanal kaçış; menzil kapanmasını yavaşlatır.
// Yön P kontrolüyle tutulur: düzeltme = kp × (ATA − hedef ATA).
// Taraf girişte (enter) crank_side_for ile seçilir ve crank boyunca sabit kalır.
class Crank : public Maneuver
{
public:
    Phase phase() const override { return Phase::Crank; }
    void enter(const Aircraft& own) override;
    FlightCommand update(const Aircraft& own) override;

    // Düşman hedef ATA'ya tolerance_deg içinde oturdu mu (gimbal sınırında tutuluyor mu)
    bool in_position(const Aircraft& own, double tolerance_deg = 5.0) const;

    // Girişte seçilen taraf
    CrankSide side() const { return side_; }

private:
    static constexpr double OFFSET_DEG = 5.0;              // derece, gimbal sınırından içeri pay
    static constexpr double KP = 1.0;                      // birimsiz: hatanın kaç katı dön
    static constexpr double MAX_CORRECTION_DEG = 180.0;    // derece, tek seferde istenen en büyük dönüş

    // derece ±180, düşmanın burnuma göre tutulacağı açı: Left'te +, Right'ta −
    double target_ata_deg(const Aircraft& own) const;

    CrankSide side_ = CrankSide::Left;   // enter()'da seçilir
};

// Beam: düşman uçağa dik uç (3/9 hattı). Füzeye göre olan notch ayrı bir manevra olacak.
// NOT: taraf kuzeye göre seçiliyor (bearing > 0 ise bearing − 90), uçağın burnuna göre değil;
// uçak bazen 180°'ye yakın ters tarafa döner. Doğrusu: iki beam yönünden şu anki yöne yakın olanı seçmek.
class Beam : public Maneuver
{
public:
    Phase phase() const override { return Phase::Beam; }
    FlightCommand update(const Aircraft& own) override;
};

// Her otopilotun ayrı manevra nesneleri vardır; her biri bir kez oluşur ve kendi durumunu korur.
// Yeni manevra: sınıfını yukarıya yaz, buraya bir satır ekle.
struct Maneuvers
{
    Hold hold;
    Fire fire;
    Out out;
    In in;
    Crank crank;   // taraf girişte seçilir
    Beam beam;
};
