#pragma once

#include "common/ned.hpp"
#include "model/kinematics.hpp"
#include "model/picture.hpp"

// Sabit yük faktörüyle seviye dönüş. Saf fonksiyonlar; out, commit, recommit hesapları bunun üstüne kurulur.
// Her fonksiyon tek bir şeyi hesaplar; to_heading hepsini birleştirip TurnResult döner.
namespace turn
{
    // m, dönüş yarıçapı: r = V² / (g·√(n²−1))
    double radius_m(double speed_kt, double load_factor_g);

    // derece ±180, start_heading_deg'den target_heading_deg'e kısa taraftan dönüş açısı; + sağa, − sola
    double angle_deg(double start_heading_deg, double target_heading_deg);

    // kt, dönüş boyunca ortalama hız: hız başlangıçtan bitişe doğrusal değişir kabul edilir
    double average_speed_kt(double start_speed_kt, double end_speed_kt);

    // s, dönüş süresi: Δt = |Δψ| · r / V
    double time_s(double turn_deg, double radius_m, double speed_kt);

    // NM, dönüş bitince başlangıca göre yer değişimi; down hep 0.
    //   s: sağa +1, sola −1;  ψ0 başlangıç, ψ1 bitiş yönü
    //   kuzey = s · r · (sin ψ1 − sin ψ0),  doğu = s · r · (cos ψ0 − cos ψ1)
    Ned displacement_nm(double start_heading_deg, double turn_deg, double radius_m);

    // own'un şu anki yönünden target_heading_deg'e, kısa taraftan, sabit g ile dönüş.
    // Yarıçap ve süre ortalama hızla (şu anki TAS ile end_speed_kt'nin ortası) hesaplanır.
    TurnResult to_heading(const Kinematics& own, double target_heading_deg, double load_factor_g, double end_speed_kt);
}
