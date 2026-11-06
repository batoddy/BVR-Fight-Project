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
    bool nose_on_target(const Aircraft& own);      // burnum düşmana dönük: |ATA| ≤ 3°
    bool inside_enemy_mar(const Aircraft& own);    // ben düşmanın MAR'ının içindeyim: düşman şimdi out yapmalı (picture.me_in_enemy_mar)
    bool enemy_in_my_wez(const Aircraft& own);     // düşman benim WEZ'imde
    bool enemy_in_my_nez(const Aircraft& own);     // düşman benim NEZ'imde: şimdi atarsam out yapsa da kaçamaz
    bool enemy_in_my_tr(const Aircraft& own);      // menzil ≤ TR (weapons.enemy_in_tr)
    Aspect enemy_aspect(const Aircraft& own);      // düşmanın burnu bana göre (picture.ta_deg)
    Aspect my_aspect(const Aircraft& own);         // benim burnum düşmana göre (picture.ata_deg)
    bool is_spiked(const Aircraft& own);           // bana gelen düşman füzesi var (radar kilidi modeli yokken)
    bool own_missile_pitbull(const Aircraft& own, int fire_order);   // fire_order'ıncı füzem (1: ilk) pitbull; o füze yoksa false
}
