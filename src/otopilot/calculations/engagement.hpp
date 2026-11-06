#pragma once

#include <optional>

#include "model/aircraft.hpp"
#include "model/missile_profiles.hpp"
#include "model/picture.hpp"
#include "model/weapons.hpp"

// Füze ve taktik hesapları: WEZ, A/F-pole, TR, out planı, R_cold, NEZ, MAR, DR, DOR. Saf fonksiyonlar.
//
// geometry'den farkı: yalnız konuma değil, uçak ve füze profillerine de bakar.
// Hepsi own'un gözünden (own, enemy) alır ve gereken her şeyi iki uçaktan kendisi çeker.
// Yalnız ölçülen (kinematics) ve sabit (profile) alanları okur; picture ve weapons'a bakmaz.
//   Saldırı: own'un ana silahı enemy'ye karşı → wez_nm, in_wez, a_pole_nm, f_pole_nm, tr_nm
//   Kaçış:   own, enemy'nin ana silahından     → out_plan, r_cold_nm, nez_nm, mar_nm
// Uçaklar ters verilince düşmanın gözünden hesaplanır: nez_nm(enemy, own) = düşman füzemden kaçamaz.
namespace engagement
{
    /// own'un ana silahının WEZ'i.
    ///   max_nm: düşman bugünkü rotasını sürdürürse azami menzil; düşman bana geliyorsa uzar, kaçıyorsa kısalır
    Wez wez_nm(const Aircraft& own, const Aircraft& enemy);

    /// enemy, own'un WEZ'inde mi: min_nm < menzil < max_nm
    bool in_wez(const Aircraft& own, const Aircraft& enemy);

    /// NM, A-pole; füze pitbull mesafesine erişemezse std::nullopt.
    std::optional<double> a_pole_nm(const Aircraft& own, const Aircraft& enemy);

    /// NM, F-pole; füze hedef mesafesine erişemezse std::nullopt.
    std::optional<double> f_pole_nm(const Aircraft& own, const Aircraft& enemy);

    /// NM, TR; füze pitbull mesafesine erişemezse std::nullopt.
    std::optional<double> tr_nm(const Aircraft& own, const Aircraft& enemy);

    /// own şimdi out yapsa: enemy'yi kuyruğa alan dönüş, kaçış hızı ve dönüşte enemy'ye yaklaşma.
    OutPlan out_plan(const Aircraft& own, const Aircraft& enemy);

    /// Aynı plan, düşman burnuma göre ata_deg'deyken out yapılırsa.
    OutPlan out_plan(const Aircraft& own, const Aircraft& enemy, double ata_deg);

    /// NM, R_cold: own baştan kaçıyor olsaydı enemy'nin füzesinin yetişebileceği en büyük mesafe.
    double r_cold_nm(const Aircraft& own, const Aircraft& enemy);

    /// NM, No Escape Zone: enemy'nin füzesi own'un out dönüşü başladığı an atılırsa, bu mesafenin içinde own kaçamaz.
    double nez_nm(const Aircraft& own, const Aircraft& enemy);

    /// Aynı NEZ, düşman burnuma göre ata_deg'deyken out yapılırsa.
    double nez_nm(const Aircraft& own, const Aircraft& enemy, double ata_deg);

    /// NM, Minimum Abort Range: dönüşün herhangi bir anında atılan füzeden own'un kaçabileceği en kısa mesafe.
    double mar_nm(const Aircraft& own, const Aircraft& enemy);

    /// Aynı MAR, düşman burnuma göre ata_deg'deyken out yapılırsa.
    double mar_nm(const Aircraft& own, const Aircraft& enemy, double ata_deg);

    /// own, enemy'ye karşı MAR'ının içinde mi: menzil ≤ MAR
    bool in_mar(const Aircraft& own, const Aircraft& enemy);

    /// NM, Minimum Recommit Range, Cold Ops için: recommit sonrası atış açısından yapılacak out dönüşüne göre hesaplanır.
    double mrr_cold_ops_nm(const Aircraft& own, const Aircraft& enemy);

    /// NM, Decision Range: R_cold + notch süresince düşmanın yaklaşması + 0,5 NM.
    double dr_nm(const Aircraft& own, const Aircraft& enemy);

    /// NM, DOR: DR + düşmanın 180° recommit dönüşü boyunca yaklaşması + 0,5 NM.
    double dor_nm(const Aircraft& own, const Aircraft& enemy);

    /// own, enemy'ye karşı DOR'ının içinde mi: menzil ≤ DOR
    bool in_dor(const Aircraft& own, const Aircraft& enemy);

    /// NM, tüm füzelerin uzak durma menzillerini sağlayan en kısa menzil: en büyük stay_away_nm.
    /// Dışarıdan yaklaşırken ilk varılan sınır da budur. Hiçbir füzede değer yoksa std::nullopt.
    std::optional<double> closest_stay_away_nm(const std::vector<ThreatInfo>& threats);
}
