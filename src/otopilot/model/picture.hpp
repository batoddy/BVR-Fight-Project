#pragma once

#include <optional>

#include "common/ned.hpp"

// Sabit g'li seviye dönüşün sonucu | turn::to_heading
struct TurnResult
{
    double turn_deg = 0;          // derece, dönülen açı; + sağa (saat yönü), - sola
    double radius_m = 0;          // m, dönüş yarıçapı (ortalama hızla)
    double time_s = 0;            // s, dönüş süresi
    Ned displacement_nm;          // NM, dönüş bitince başlangıca göre yer değişimi; down hep 0
    double end_heading_deg = 0;   // derece ±180, dönüş sonu yönü
};

// Out manevrasının tahmini | engagement::out_plan. R_cold, NEZ ve MAR bunu kullanır.
struct OutPlan
{
    TurnResult turn;            // düşmanı kuyruğa alan dönüş: bearing + 180'e, kısa taraftan
    double escape_kt = 0;       // kt, dönüş sonu kaçış hızı
    double approach_nm = 0;     // NM, dönüş bitince düşmana yatay yaklaşma; + yaklaştık
};

// Bana doğru gelen düşman füzesinin bana göre durumu | World::update
struct ThreatInfo
{
    int slot = 0;                          // düşmanın füze slotu: enemy.weapons.missiles[slot]
    double range_nm = 0;                   // NM, füze ile aramızdaki 3B mesafe
    double closure_kt = 0;                 // kt, füzenin bana kapanma hızı; + yaklaşıyor
    double bearing_deg = 0;                // derece ±180, benden füzeye yön; 0 kuzey
    double ata_deg = 0;                    // derece ±180, burnuma göre füze; + sağımda
    double aspect_deg = 0;                 // derece ±180, füzenin burnu bana göre; 0 bana dönük
    double mach = 0;                       // birimsiz, füzenin hızı
    bool is_pitbull = false;               // füzenin arayıcısı beni kendi görüyor mu
};

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
    double aspect_deg = 0;         // derece ±180, düşmanın burnu ile bana bakışı; 0 hot, 180 cold
    Ned los_nm;                    // NM, benden düşmana vektör (uzunluğu range_3d_nm)
    Ned los_unit;                  // birimsiz, aynı vektörün yönü
    double closure_kt = 0;         // kt, mesafenin azalma hızı; + yaklaşıyoruz, ikimizin hareketi
    double enemy_closure_kt = 0;   // kt, yalnız düşmanın bana doğru hızı; + bana geliyor

    // Kaçış: düşmanın füzesine karşı | engagement::
    OutPlan out;                       // şimdi out yapsam
    bool me_in_enemy_wez = false;      // ben düşmanın WEZ'inde miyim (onun weapons.enemy_in_wez'i)
    double r_cold_nm = 0;              // NM, kaçarken düşman füzesinin (main_weapon) yetişebileceği mesafe
    double nez_nm = 0;                 // NM, düşmanın füzesinden kaçamayacağım mesafe (onun NEZ'i)
    double mar_nm = 0;                 // NM, Minimum Abort Range
    double dor_nm = 0;                 // NM, Desired Out Range: out sonrası recommit edebileceğim mesafe
    bool is_in_mar = false;            // range_3d_nm <= mar_nm

    // Bana gelen en yakın düşman füzesi; yoksa yok
    std::optional<ThreatInfo> threat;
};
