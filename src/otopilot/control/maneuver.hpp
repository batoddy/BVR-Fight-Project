#pragma once

#include "model/aircraft.hpp"
#include "model/orders.hpp"

// Bütün manevraların ortak arayüzü. Manevra yalnız "bu durumda nasıl uçarım"
// sorusunu cevaplar. Hangisinin çalışacağını ilgili uçak kontrolcüsü seçer.
class Maneuver
{
public:
    virtual ~Maneuver() = default;

    // fighter: ölçüm (kinematics) ve hesaplanan bilgiler (enemy)
    // current: fighter'ın şu anki komutu; manevra yalnız ilgilendiği alanı değiştirir
    virtual FlightCommand compute(const Aircraft& fighter, const FlightCommand& current) = 0;
};

// Mevcut komutu aynen koru: aynı yön, irtifa ve gazla devam.
class Hold : public Maneuver
{
public:
    FlightCommand compute(const Aircraft& fighter, const FlightCommand& current) override;

private:
    // Manevranın kendi ayarları ve durumu burada durur.
};


class Out : public Maneuver
{
public:
    // ilk girişte çağrılır
    void begin(const Aircraft& fighter);

    FlightCommand compute(const Aircraft& fighter, const FlightCommand& current) override;

private:
    double descend_ft_ = 3300.0;       // ft, out'ta ne kadar alçalınacak
    double floor_ft_ = 10000.0;        // ft, out bunun altına indirmez
    double target_altitude_ft_ = 0;    // ft, begin()'de bir kez hesaplanır, out boyunca sabit
};

class In : public Maneuver
{
public:
    FlightCommand compute(const Aircraft& fighter, const FlightCommand& current) override;

private:
    // Manevranın kendi ayarları ve durumu burada durur (ör. hangi tarafa crank, pay açısı).
};

// Crank yönü, havacılık terimiyle: uçağın döndüğü taraf.
enum class CrankSide
{
    Left,    // uçak sola döner, düşman burnun SAĞINDA kalır (hedef ATA +)
    Right,   // uçak sağa döner, düşman burnun SOLUNDA kalır (hedef ATA -)
};

// Crank: düşmanı radar gimbal sınırının biraz içinde tutarak yanal kaçış; menzil kapanmasını yavaşlatır.
// Yön P kontrolüyle tutulur: düzeltme = kp × (ata - hedef_ata).
// kp = 1 eski formülle aynı; kp büyüdükçe hareketli hedefin gerisinde kalma azalır.
class Crank : public Maneuver
{
public:
    explicit Crank(CrankSide side) : side_(side) {}

    FlightCommand compute(const Aircraft& fighter, const FlightCommand& current) override;

private:
    CrankSide side_;
    double offset_deg_ = 5.0;           // derece, gimbal sınırından içeri pay
    double kp_ = 1.0;                   // birimsiz: hatanın kaç katı dön
    double max_correction_deg_ = 180.0;  // derece, tek seferde istenen en büyük dönüş
};

class Notch : public Maneuver
{
public:

    FlightCommand compute(const Aircraft& fighter, const FlightCommand& current) override;

private:
};





// Her uçak kontrolcüsünün ayrı manevra nesneleri vardır.
// Her biri bir kez oluşur ve kendi durumunu korur.
// Yeni manevra: sınıfını yukarıya yaz, buraya bir satır ekle.
struct Maneuvers
{
    Hold hold;
    Out out;
    In in;
    Crank crank_left{CrankSide::Left};     // düşman sağda, uçak sola döner
    Crank crank_right{CrankSide::Right};   // düşman solda, uçak sağa döner
    Notch notch;
};
