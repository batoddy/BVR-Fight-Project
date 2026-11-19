#pragma once

#include "model/aircraft.hpp"
#include "model/flight_command.hpp"
#include "model/phase.hpp"

// ORTAK  —  bütün manevraların arayüzü

enum class ManeuverStatus
{
    Running,   // sürüyor
    Done,      // kendi hedefine ulaştı (yalnız kendi içinde biten manevralar)
    Satisfied, // sürüyor ve şu an istenen koşulu sağlıyor; koşul bozulunca Running'e döner
};

// Manevra yalnız "bu durumda nasıl uçarım" sorusunu cevaplar; hangisinin çalışacağını otopilot seçer.
// Gaz ve g sınırı manevranın içindedir: her manevra kendi THROTTLE (throttle::) ve G_LIMIT değerini yazar.
class Maneuver
{
public:
    virtual ~Maneuver() = default;

    virtual Phase phase() const = 0;                                   // görüntüleyicide ve CSV'de görünen ad
    virtual void enter(const Aircraft& own) { (void)own; }             // manevraya geçilince bir kez: girişe bağlı hedefleri kilitler
    virtual FlightCommand update(const Aircraft& own) = 0;             // her step: own.command'dan başlayıp yeni komutu döner
    virtual ManeuverStatus status(const Aircraft& own) const           // varsayılan Running; açı tutanlar Satisfied, bitenler Done bildirebilir
    {
        (void)own;
        return ManeuverStatus::Running;
    }

    bool is_satisfied(const Aircraft& own) const                      // yalnız mevcut koşulun sağlanması; Done ayrı bir durumdur
    {
        return status(own) == ManeuverStatus::Satisfied;
    }
};

// Manevra yönü, havacılık terimiyle: uçağın döndüğü taraf.
enum class ManeuverSide
{
    Right,   // uçak sağa döner. Crank: düşman burnun SOLUNDA kalır (ATA −). Recommit: SAĞINDA kalır (ATA +).
    Left,    // uçak sola döner. Crank: düşman burnun SAĞINDA kalır (ATA +). Recommit: SOLUNDA kalır (ATA −).
};
