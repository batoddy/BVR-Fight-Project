#pragma once

#include "model/aircraft.hpp"

// 1v1 dünya: kendi uçağımız ve saptadığımız düşman.
struct World
{
    double sim_time_s = 0;   // s, JSBSim saati | data.sim_time_s | simulation/sim-time-sec

    Aircraft fighter;        // biz     (roles.hpp FIGHTER_ID)
    Aircraft hostile;          // düşman  (roles.hpp HOSTILE_ID)

    // Ölçüm doldurulduktan sonra her step çağrılır: iki uçağın hesaplanan alanlarını doldurur.
    void update();
};
