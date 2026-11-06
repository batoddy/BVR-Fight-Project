#pragma once

#include <optional>
#include <vector>

#include "common/ned.hpp"
#include "model/turn_result.hpp"

// Out manevrasının tahmini | engagement::out_plan. R_cold, NEZ ve MAR bunu kullanır.
struct OutPlan
{
    TurnResult turn;            // düşmanı kuyruğa alan dönüş: bearing + 180'e, kısa taraftan
    double escape_kt = 0;       // kt, dönüş sonu kaçış hızı
    double approach_nm = 0;     // NM, dönüş bitince düşmana yatay yaklaşma; + yaklaştık
};

struct Missile;   // model/missile.hpp; tehdit listesi yalnız adres tutar

// own'un atanmış düşmana göre durum resmi: geometri ve kaçış (düşmanın füzesine karşı).
// Ölçülmez, hesaplanır | World::update
//   own.picture.range_3d_nm, own.picture.mar_nm, own.picture.threat ...
// Kendi füzemin düşmana karşı durumu (WEZ, NEZ, F-pole) own.weapons'tadır.
struct Picture
{
    // Geometri | geometry::
    double range_2d_nm = 0;        // NM, yatay mesafe (irtifa farkı yok)
    double range_3d_nm = 0;        // NM, 3B düz çizgi mesafe
    double bearing_deg = 0;        // derece ±180, kuzeye göre düşmanın yönü
    double ata_deg = 0;            // derece ±180, burnum ile düşman arası; + sağımda
    double ta_deg = 0;             // derece ±180, Target Aspect: düşmanın burnu ile bana bakışı; 0 hot, 180 cold
    Ned los_nm;                    // NM, benden düşmana vektör (uzunluğu range_3d_nm)
    Ned los_unit;                  // birimsiz, aynı vektörün yönü
    double closure_kt = 0;         // kt, mesafenin azalma hızı; + yaklaşıyoruz, ikimizin hareketi
    double enemy_closure_kt = 0;   // kt, yalnız düşmanın bana doğru hızı; + bana geliyor
    double los_rate_deg_s = 0;     // derece/s, bearing'in değişimi; + saat yönünde
    Ned enemy_velocity_ned_kt;     // kt, hedefin yer hızı; hareketli yan konum takibi için | World::update

    // Kaçış: düşmanın füzesine karşı | engagement::
    OutPlan out;                       // şimdi out yapsam
    bool me_in_enemy_wez = false;      // ben düşmanın WEZ'inde miyim (onun weapons.enemy_in_wez'i)
    bool me_in_enemy_mar = false;      // ben düşmanın MAR'ının içinde miyim: düşman şimdi out yapmalı (onun picture.is_in_mar'ı)
    std::optional<double> enemy_a_pole_nm;   // NM, düşman şimdi atsa A-pole'u (onun weapons.a_pole_nm'i)
    std::optional<double> enemy_f_pole_nm;   // NM, düşman şimdi atsa F-pole'u (onun weapons.f_pole_nm'i)
    double r_cold_nm = 0;              // NM, kaçarken düşman füzesinin (main_weapon) yetişebileceği mesafe
    double nez_nm = 0;                 // NM, düşmanın füzesinden kaçamayacağım mesafe (onun NEZ'i)
    double mar_nm = 0;                 // NM, Minimum Abort Range
    double dr_nm = 0;                  // NM, Decision Range: R_cold + notch süresince düşmanın TAS'ıyla yaklaşması + 0,5 NM
    double dor_nm = 0;                 // NM, DOR: MAR + recommit dönüşüm süresince düşmanın TAS'ıyla yaklaşması + 0,5 NM
    bool is_in_mar = false;            // range_3d_nm <= mar_nm
    bool is_in_dor = false;            // range_3d_nm <= dor_nm

    // Tehditler: enemy.weapons.missiles içine adres; her step baştan taranır. Bilgileri missile->target'ta; sonraki step'e saklanmaz.
    std::vector<const Missile*> threats;   // bana gelen bütün düşman füzeleri; yoksa boş
    const Missile* threat = nullptr;       // bana en yakın olanı; yoksa nullptr
    bool is_spiked = false;                // bana füze geliyor mu. NOT: gerçek spike radar kilididir, radar modeli yok
};
