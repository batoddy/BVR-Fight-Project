#pragma once

#include "control/maneuvers/maneuver.hpp"

// FLANK  —  düşmanın burnuna göre verilen yan açıyı koru

// maneuvers.flank.set(40.0): düşmanın 40° sağı; set(-40.0): 40° solu. Açıya varınca da uçmaya devam eder.
class Flank : public Maneuver
{
public:
    Phase phase() const override { return Phase::Flank; }
    void set(double target_ta_deg, double tolerance_deg = 3.0);    // hedef TA ±180'e sarılır; tolerans 0..180 derece olmalıdır
    void enter(const Aircraft& own) override;                     // girişteki yatay menzil kilitlenir
    FlightCommand update(const Aircraft& own) override;           // hareketli hedefin verilen yan konumuna, kilitli menzilde yönelir; irtifa aynı
    ManeuverStatus status(const Aircraft& own) const override;    // TA hedefin toleransı içindeyse Satisfied, dışında Running; Done olmaz

private:
    static constexpr double THROTTLE = throttle::DEFAULT;
    static constexpr double G_LIMIT = 4.0;                        // g, konum düzeltme dönüşü
    static constexpr double RESPONSE_S = 20.0;                    // s, yan konum hatasını giderme süresi; simülasyon ayarı

    double target_ta_deg_ = 50.0;                                // derece, düşmanın burnuna göre istenen işaretli açı | set
    double tolerance_deg_ = 3.0;                                 // derece, hedef açı çevresindeki kabul aralığı | set
    double range_nm_ = 0;                                        // NM, düşmandan tutulacak yatay menzil | enter
};
