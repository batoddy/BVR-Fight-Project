#pragma once

#include "control/maneuvers/maneuver.hpp"

// OUT  —  düşmana kuyruk dön, alçal, tam art yakıcı; düşman arkada kalınca Done, uçulmaya devam ederse kaçış sürer

class Out : public Maneuver
{
public:
    Phase phase() const override { return Phase::Out; }
    void enter(const Aircraft& own) override;                     // hedef irtifayı kilitler: giriş − DESCEND_FT, FLOOR_FT altına inme, tırmanma yok
    FlightCommand update(const Aircraft& own) override;           // yön = bearing − 180, irtifa = kilitli hedef, out g'si
    ManeuverStatus status(const Aircraft& own) const override;    // |ATA| > DONE_ATA_DEG: düşman arkada, Done

private:
    static constexpr double DESCEND_FT = 3300.0;                  // ft, out'ta ne kadar alçalınacak
    static constexpr double FLOOR_FT = 10000.0;                   // ft, out bunun altına indirmez
    static constexpr double TURN_THROTTLE = throttle::MAX_THRUST; // dönüşte art yakıcı
    static constexpr double RUN_THROTTLE = throttle::MAX_THRUST;         // dönüş bitince askeri güç: COLD ve ABORT aynı out'u sürdürür
    static constexpr double DONE_ATA_DEG = 177.0;                 // derece, |ATA| bunu geçince düşman arkada

    double target_altitude_ft_ = 0;                               // ft, enter()'da bir kez hesaplanır
    bool turn_done_ = false;                                      // dönüş bir kez bitti mi; enter()'da sıfırlanır
};
