#pragma once

#include "common/ned.hpp"
#include "model/kinematics.hpp"
#include "model/turn_result.hpp"

// Sabit yük faktörüyle seviye dönüş. Saf fonksiyonlar; out, commit, recommit hesapları bunun üstüne kurulur.
// Her fonksiyon tek bir şeyi hesaplar; to_heading hepsini birleştirip TurnResult döner.
//   yarıçap      r = V² / (g·√(n²−1))
//   süre         Δt = |Δψ| · r / V
//   yer değişimi kuzey = s · r · (sin ψ1 − sin ψ0),  doğu = s · r · (cos ψ0 − cos ψ1)   (s: sağa +1, sola −1)
namespace turn
{
    double radius_m(double speed_kt, double load_factor_g);                             // m, dönüş yarıçapı
    double angle_deg(double start_heading_deg, double target_heading_deg);              // derece ±180, kısa taraftan dönüş; + sağa, − sola
    double average_speed_kt(double start_speed_kt, double end_speed_kt);                // kt, hız başlangıçtan bitişe doğrusal değişir kabul edilir
    double time_s(double turn_deg, double radius_m, double speed_kt);                   // s, dönüş süresi
    Ned displacement_nm(double start_heading_deg, double turn_deg, double radius_m);    // NM, dönüş sonu yer değişimi; down hep 0

    // own'un yönünden target_heading_deg'e kısa taraftan, sabit g ile; yarıçap ve süre ortalama hızla (TAS ile end_speed_kt'nin ortası).
    TurnResult to_heading(const Kinematics& own, double target_heading_deg, double load_factor_g, double end_speed_kt);
}
