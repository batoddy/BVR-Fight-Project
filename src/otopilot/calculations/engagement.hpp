#pragma once

#include <optional>

#include "model/aircraft.hpp"
#include "model/missile_profiles.hpp"
#include "model/picture.hpp"
#include "model/weapons.hpp"

// Füze ve taktik hesapları: WEZ, F-pole, out planı, R_cold, NEZ, MAR, DOR. Saf fonksiyonlar.
//
// geometry'den farkı: yalnız konuma değil, uçak ve füze profillerine de bakar.
// Hepsi own'un gözünden (own, enemy) alır ve gereken her şeyi iki uçaktan kendisi çeker.
// Yalnız ölçülen (kinematics) ve sabit (profile) alanları okur; picture ve weapons'a bakmaz.
//   Saldırı: own'un ana silahı enemy'ye karşı → wez, in_wez, f_pole_nm
//   Kaçış:   own, enemy'nin ana silahından     → out_plan, r_cold_nm, nez_nm, mar_nm
// Uçaklar ters verilince düşmanın gözünden hesaplanır: nez_nm(enemy, own) = düşman füzemden kaçamaz.
namespace engagement
{
    // own'un ana silahının WEZ'i.
    //   max_nm: düşman bugünkü rotasını sürdürürse azami menzil; düşman bana geliyorsa uzar, kaçıyorsa kısalır
    Wez wez(const Aircraft& own, const Aircraft& enemy);

    // enemy, own'un WEZ'inde mi: min_nm < menzil < max_nm
    bool in_wez(const Aircraft& own, const Aircraft& enemy);

    // NM, F-pole: own'un füzesi enemy'ye vardığında iki uçak arasında kalan mesafe. Füze yetişemiyorsa yok.
    std::optional<double> f_pole_nm(const Aircraft& own, const Aircraft& enemy);

    // own şimdi out yapsa: enemy'yi kuyruğa alan dönüş, kaçış hızı ve dönüşte enemy'ye yaklaşma.
    OutPlan out_plan(const Aircraft& own, const Aircraft& enemy);

    // NM, R_cold: own baştan kaçıyor olsaydı enemy'nin füzesinin yetişebileceği en büyük mesafe.
    double r_cold_nm(const Aircraft& own, const Aircraft& enemy);

    // NM, No Escape Zone: enemy'nin füzesi own'un out dönüşü başladığı an atılırsa, bu mesafenin içinde own kaçamaz.
    double nez_nm(const Aircraft& own, const Aircraft& enemy);

    // NM, Minimum Abort Range: dönüşün herhangi bir anında atılan füzeden own'un kaçabileceği en kısa mesafe.
    double mar_nm(const Aircraft& own, const Aircraft& enemy);

    // own, enemy'ye karşı MAR'ının içinde mi: menzil ≤ MAR
    bool in_mar(const Aircraft& own, const Aircraft& enemy);

    double dr_nm();                  
    // NM, Desired Out Range: out yaptıktan sonra geri dönüp tekrar angaje olabileceğim (recommit) mesafe.
    double dor_nm(const Aircraft& own, const Aircraft& enemy);
}
