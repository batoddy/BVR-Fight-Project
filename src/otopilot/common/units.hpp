#pragma once

// Birimler, dönüşümler ve fiziksel sabitler.
// C++ içinde: irtifa ft, mesafe NM, hız kt, süre s, açı derece.
// Python uçuş mesajları metre ve m/s kullanır; dönüşüm IO sınırında yapılır.

inline constexpr double NMILE_TO_METER = 1852.0;
inline constexpr double METER_TO_NMILE = 1.0 / NMILE_TO_METER;
inline constexpr double FOOT_TO_METER = 0.3048;
inline constexpr double SECONDS_PER_HOUR = 3600.0;

inline constexpr double GRAVITY_MPS2 = 9.81;
inline constexpr double PI = 3.14159265358979323846;   // MinGW c++17'de M_PI yok
inline constexpr double DEG_TO_RAD = PI / 180.0;
inline constexpr double RAD_TO_DEG = 180.0 / PI;

inline constexpr double meter_to_nmile(double m) { return m * METER_TO_NMILE; }
inline constexpr double nmile_to_meter(double nm) { return nm * NMILE_TO_METER; }
inline constexpr double meter_to_feet(double m) { return m / FOOT_TO_METER; }
inline constexpr double feet_to_meter(double ft) { return ft * FOOT_TO_METER; }
inline constexpr double feet_to_nmile(double ft) { return meter_to_nmile(feet_to_meter(ft)); }
inline constexpr double nmile_to_feet(double nm) { return meter_to_feet(nmile_to_meter(nm)); }
inline constexpr double mps_to_knot(double mps) { return meter_to_nmile(mps) * SECONDS_PER_HOUR; }
inline constexpr double knot_to_mps(double kt) { return nmile_to_meter(kt) / SECONDS_PER_HOUR; }
inline constexpr double fps_to_knot(double fps) { return mps_to_knot(feet_to_meter(fps)); }
inline constexpr double knot_to_fps(double kt) { return meter_to_feet(knot_to_mps(kt)); }

// Yol = hız × süre. kt "saatte NM" olduğu için süre saniyeden saate çevrilir.
inline constexpr double travel_distance_nm(double speed_kt, double time_s) { return speed_kt * time_s / SECONDS_PER_HOUR; }

// Süre = yol / hız, saniye.
inline constexpr double travel_time_s(double distance_nm, double speed_kt) { return SECONDS_PER_HOUR * distance_nm / speed_kt; }
