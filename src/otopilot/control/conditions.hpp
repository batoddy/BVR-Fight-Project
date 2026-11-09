#pragma once

#include "model/aircraft.hpp"

// Bir uçağın burnunun diğer uçağa göre durumu. Yalnız açının büyüklüğüne bakılır; sağ/sol ayrılmaz.
enum class Aspect
{
    Hot,     // |açı| <= 30°: burnu karşıya dönük, üstüne geliyor
    Flank,   // 30° < |açı| <= 70°: çaprazdan geliyor
    Beam,    // 70° < |açı| <= 110°: yana uçuyor
    Cold,    // |açı| > 110°: sırtı dönük, uzaklaşıyor
};

// Otopilot geçiş koşulları: isimli, saf sorgular. Karar yapısı (switch, karar ağacı, ...) değişse de
// koşullar aynı kalır. Yalnız own'un picture ve weapons alanlarını okur; hesap yapmaz.
namespace condition
{
    bool in_mar(const Aircraft& own);              // menzil MAR'ımın içinde: şimdi out yapmazsam kaçamayabilirim
    bool in_dor(const Aircraft& own);              // menzil DOR'umun içinde
    bool enemy_in_my_wez(const Aircraft& own);     // düşman benim WEZ'imde
    bool enemy_in_my_tr(const Aircraft& own);      // menzil ≤ TR (weapons.enemy_in_tr)
    bool enemy_in_my_msr(const Aircraft& own);     // menzil ≤ MSR (weapons.enemy_in_msr)
    bool enemy_in_my_r_cold(const Aircraft& own);  // menzil ≤ R_cold'um: kaçan düşmana füzem yetişir
    bool beyond_mrr(const Aircraft& own);          // menzil ≥ MRR: recommit yapıp atarsam sonraki out'ta kaçabilirim
    Aspect enemy_aspect(const Aircraft& own);      // düşmanın burnu bana göre (picture.ta_deg)
    Aspect my_aspect(const Aircraft& own);         // benim burnum düşmana göre (picture.ata_deg)
    bool enemy_cold(const Aircraft& own);          // düşman kaçıyor: Cold bandı (TA > 110°)
    bool enemy_hot(const Aircraft& own);           // düşman bana dönük: Hot ya da Flank (TA ≤ 70°); aradaki Beam bandı histerezis
    bool has_live_missile(const Aircraft& own);    // atış emrim bekliyor ya da aktif füzem var (weapons.launch_pending, active_missile)
    bool out_of_missiles(const Aircraft& own);     // atılacak füzem kalmadı ve canlı füzem yok
    bool has_guided_threat(const Aircraft& own);   // bana gelen güdümlü füze var (picture.threat yalnız güdümlüleri seçer)
    bool threat_harmless(const Aircraft& own);     // tehdit benden yavaş ve füzeye dönsem bile recommit payı yetiyor
    bool threat_alive(const Aircraft& own);        // bana gelen güdümlü, yetişebilen ve zararsız olmayan füze var
    bool supporting(const Aircraft& own);          // atış emrim bekliyor ya da desteklediğim füze henüz pitbull değil
    bool is_spiked(const Aircraft& own);           // bana gelen düşman füzesi var (radar kilidi modeli yokken)
}
