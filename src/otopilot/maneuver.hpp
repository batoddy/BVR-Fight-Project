#pragma once

#include "aircraft.hpp"
#include "orders.hpp"

// Bütün manevraların ortak arayüzü. Manevra yalnız "bu durumda nasıl uçarım"
// sorusunu cevaplar. Hangisinin çalışacağı şimdilik Autopilot::step içinde elle seçilir;
// karar mekanizması eklenince seçim oraya taşınır.
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

// Hedefi radar gimbal sınırına yakın tutarak yanal kaçış; menzil kapanmasını yavaşlatır.
class Crank : public Maneuver
{
public:
    FlightCommand compute(const Aircraft& fighter, const FlightCommand& current) override;

private:
    // Manevranın kendi ayarları ve durumu burada durur (ör. hangi tarafa crank, pay açısı).
};

// Bütün manevralar bir arada. Her biri bir kez oluşur ve kendi durumunu korur.
// Yeni manevra: sınıfını yukarıya yaz, buraya bir satır ekle.
struct Maneuvers
{
    Hold hold;
    Crank crank;
};
