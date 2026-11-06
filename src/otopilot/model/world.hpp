#pragma once

#include "model/aircraft.hpp"

// 1v1 dünya: kendi uçağımız ve düşman. Düşman ataması ve bütün hesaplanan alanlar burada doldurulur.
// Kopyalanmaz: picture.threat/threats karşı uçağın füzelerine işaretçi tutar; kopya eski World'ü gösterirdi.
struct World
{
    World() = default;
    World(const World&) = delete;
    World& operator=(const World&) = delete;

    double sim_time_s = 0;   // s, JSBSim saati | data.sim_time_s | simulation/sim-time-sec

    Aircraft fighter;        // biz    (roles.hpp FIGHTER_ID)
    Aircraft hostile;        // düşman (roles.hpp HOSTILE_ID)

    /// Ölçüm okunduktan sonra her step çağrılır: düşman ataması, füze bilgileri ve picture.
    void update();
};
