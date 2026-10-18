#pragma once

#include "aircraft.hpp"

// 1v1 dünya: kendi uçağımız ve saptadığımız düşman.
struct World
{
    double sim_time_s = 0;   // s, JSBSim saati | data.sim_time_s | simulation/sim-time-sec

    Aircraft fighter;        // biz     (roles.hpp FIGHTER_ID)
    Aircraft bogey;          // düşman  (roles.hpp BOGEY_ID)

    // Ölçüm doldurulduktan sonra her step çağrılır: iki uçağın hesaplanan alanlarını doldurur.
    void update();
};
