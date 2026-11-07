#pragma once

#include <optional>

#include "model/aircraft.hpp"
#include "model/missile_profiles.hpp"
#include "model/picture.hpp"
#include "model/weapons.hpp"

// Füze ve taktik hesapları: WEZ, A/F-pole, TR, out planı, R_cold, NEZ, MAR, MRR, DR, DOR. Saf fonksiyonlar.
//
// geometry'den farkı: yalnız konuma değil, uçak ve füze profillerine de bakar.
// Hepsi own'un gözünden (own, enemy) alır. Çoğu yalnız ölçülen (kinematics) ve sabit (profile) alanları okur;
// A-pole, F-pole ve TR ayrıca own.picture'ı okur, bu yüzden World::update'te compute_picture'dan sonra çağrılır.
//   Saldırı: own'un ana silahı enemy'ye karşı → wez_nm, in_wez, in_nez, a_pole_nm, f_pole_nm, tr_nm, in_tr
//   Kaçış:   own, enemy'nin ana silahından     → out_plan, r_cold_nm, nez_nm, mar_nm, mrr_nm, dr_nm, dor_nm
// Uçaklar ters verilince düşmanın gözünden hesaplanır: nez_nm(enemy, own) = düşman füzemden kaçamaz.
namespace engagement
{
    // --- Saldırı: benim füzem ---
    Wez wez_nm(const Aircraft& own, const Aircraft& enemy);                     // WEZ; max_nm düşman bana geliyorsa uzar, kaçıyorsa kısalır
    bool in_wez(const Aircraft& own, const Aircraft& enemy);                    // min_nm < menzil ≤ max_nm
    bool in_nez(const Aircraft& own, const Aircraft& enemy);                    // min_nm < menzil ≤ nez_nm(enemy, own): atarsam out yapsa da kaçamaz
    std::optional<double> a_pole_nm(const Aircraft& own, const Aircraft& enemy);   // NM, füze pitbull olduğunda iki uçak arası; erişemezse yok
    std::optional<double> f_pole_nm(const Aircraft& own, const Aircraft& enemy);   // NM, füze hedefe vardığında iki uçak arası; erişemezse yok
    std::optional<double> tr_nm(const Aircraft& own, const Aircraft& enemy);       // NM, en kısa atış menzili: A-pole DOR dışında; picture.dor_nm gerekir
    bool in_tr(const Aircraft& own, const Aircraft& enemy);                     // menzil ≤ TR; TR yoksa false
    std::optional<double> msr_nm(const Aircraft& own, const Aircraft& enemy);      // NM, MSR; çözüm yoksa yok
    bool in_msr(const Aircraft& own, const Aircraft& enemy);                    // menzil ≤ MSR; MSR yoksa false

    // --- Kaçış: düşmanın füzesi ---
    OutPlan out_plan(const Aircraft& own, const Aircraft& enemy);                    // şimdi out yapsam: dönüş, kaçış hızı, dönüşte yaklaşma
    OutPlan out_plan(const Aircraft& own, const Aircraft& enemy, double ata_deg);    // aynısı, düşman burnuma göre ata_deg'deyken
    double r_cold_nm(const Aircraft& own, const Aircraft& enemy);                    // NM, baştan kaçıyor olsaydım füzenin yetişebileceği en büyük mesafe
    double nez_nm(const Aircraft& own, const Aircraft& enemy);                       // NM, füze out dönüşümün başında atılırsa bunun içinde kaçamam
    double nez_nm(const Aircraft& own, const Aircraft& enemy, double ata_deg);       // aynısı, ata_deg'den out
    double mar_nm(const Aircraft& own, const Aircraft& enemy);                       // NM, dönüşün herhangi anında atılan füzeden kaçabileceğim en kısa mesafe
    double mar_nm(const Aircraft& own, const Aircraft& enemy, double ata_deg);       // aynısı, ata_deg'den out
    bool in_mar(const Aircraft& own, const Aircraft& enemy);                         // menzil ≤ MAR
    double mrr_nm(const Aircraft& own, const Aircraft& enemy);                       // NM, Cold Ops'ta: buradan recommit yapıp atarsam sonraki out'ta kaçabilirim
    double dr_nm(const Aircraft& own, const Aircraft& enemy);                        // NM, R_cold + notch süresince düşmanın TAS'ıyla yaklaşması + 0,5 NM
    double dor_nm(const Aircraft& own, const Aircraft& enemy);                       // NM, MAR + recommit dönüşüm süresince düşmanın TAS'ıyla yaklaşması + 0,5 NM
    bool in_dor(const Aircraft& own, const Aircraft& enemy);                         // menzil ≤ DOR
}
