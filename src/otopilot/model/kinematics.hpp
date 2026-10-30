#pragma once

#include "common/ned.hpp"

// Konum ve hareket. Uçakta da atılmış füzede de aynı yapı kullanılır.
// Yorum düzeni:   birim, anlam | JSON anahtarı | Python / JSBSim kaynağı
struct Kinematics
{
    double lat_deg = 0;       // derece, WGS84 jeodezik enlem  | lat_deg   | position/lat-geod-deg
    double lon_deg = 0;       // derece, boylam                | lon_deg   | position/long-gc-deg
    double altitude_ft = 0;   // ft                            | alt_msl_m | StateReader m → ft
    double heading_deg = 0;   // derece ±180, 0 kuzey, + doğu  | yaw_deg   | attitude/psi-deg (0..360, okurken sarılır)
    double pitch_deg = 0;     // derece, + burun yukarı        | pitch_deg | attitude/theta-deg
    double roll_deg = 0;      // derece, + sağ kanat aşağı     | roll_deg  | attitude/phi-deg
    double tas_kt = 0;        // kt, gerçek hava hızı          | tas_mps   | StateReader m/s → kt
    double mach = 0;          // birimsiz                      | mach      | velocities/mach

    Ned velocity_ned_kt;      // kt, [kuzey, doğu, aşağı]      | velocity_ned_mps | StateReader m/s → kt
};
