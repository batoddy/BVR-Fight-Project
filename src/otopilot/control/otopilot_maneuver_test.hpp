#pragma once

#include <cstddef>
#include <optional>

#include "control/maneuver.hpp"
#include "model/aircraft.hpp"

// Zamanlı manevra denemesi. Saat World::sim_time_s'ten gelir; sonuç durumları geçişi kendiliğinden tetiklemez.
// Karşı uçak Hold uçar; yalnız Notch/Drag girişlerinde birer test atışı ister.
class OtopilotManeuverTest
{
public:
    static constexpr std::size_t LEG_COUNT = 18;

    OtopilotManeuverTest() = default;
    OtopilotManeuverTest(const OtopilotManeuverTest&) = delete;
    OtopilotManeuverTest& operator=(const OtopilotManeuverTest&) = delete;

    void step(Aircraft& own, Aircraft& enemy, double sim_time_s); // iki uçağın komutları ve karşı uçağın test atış emri
    std::size_t leg_index() const { return leg_index_; }         // 0 tabanlı; LEG_COUNT tamamlanmış akıştır
    const char* leg_name() const;                               // logda görünen test adı
    double elapsed_s() const { return elapsed_s_; }             // s, mevcut test adımında geçen süre
    double duration_s() const;                                  // s, adımın sabit süresi; son Hold için 0
    bool finished() const { return leg_index_ == LEG_COUNT; }
    bool shot_failed() const { return shot_failed_; }            // mevcut adımın test atışı için hazır mühimmat yoktu
    ManeuverStatus status(const Aircraft& own) const;            // uçulan manevranın anlık sonucu

private:
    Maneuver& select(Phase phase, double angle_deg, ManeuverSide side);

    Maneuvers maneuvers_, enemy_maneuvers_;
    Maneuver* active_ = nullptr;                                // maneuvers_ içindeki mevcut nesne
    std::size_t leg_index_ = 0;
    std::optional<double> leg_start_s_;
    std::optional<double> last_time_s_;
    double elapsed_s_ = 0;
    double entry_heading_deg_ = 0;                              // derece, FlyHeading hedeflerinin başlangıç yönü
    bool shot_failed_ = false;
};
