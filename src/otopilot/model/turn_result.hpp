#pragma once

#include "common/ned.hpp"

// Sabit g'li seviye dönüşün sonucu | turn::to_heading
// Hesabı calculations/turn yapar; sonuç tipi burada durur ki Picture (OutPlan) ve turn:: aynı tipi kullansın.
struct TurnResult
{
    double turn_deg = 0;          // derece, dönülen açı; + sağa (saat yönü), - sola
    double radius_m = 0;          // m, dönüş yarıçapı (ortalama hızla)
    double time_s = 0;            // s, dönüş süresi
    Ned displacement_nm;          // NM, dönüş bitince başlangıca göre yer değişimi; down hep 0
    double end_heading_deg = 0;   // derece ±180, dönüş sonu yönü
};
