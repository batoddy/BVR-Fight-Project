#pragma once

#include "model/aircraft.hpp"

// Otopilot geçiş koşulları: isimli, saf sorgular. Karar yapısı (switch, karar ağacı, ...) değişse de
// koşullar aynı kalır. Yalnız picture ve füze durumunu okur; hesap yapmaz.
namespace condition
{
    // Düşmana menzilim MAR'ımın içinde: şimdi out yapmazsam kaçamayabilirim.
    bool in_mar(const Aircraft& own);

    // Ben düşmanın MAR'ının içindeyim: düşmanın kendi hesabına göre out yapması gereken mesafe.
    bool inside_enemy_mar(const Aircraft& own, const Aircraft& enemy);

    // Düşman benim WEZ'imde.
    bool enemy_in_my_wez(const Aircraft& own);

    // Düşman benim NEZ'imde: şimdi atarsam out yapsa da kaçamaz.
    bool enemy_in_my_nez(const Aircraft& own);

    // Düşman benden uzaklaşıyor: burnu bana dönük değil (|aspect| > 90°).
    bool enemy_cold(const Aircraft& own);

    // MAR'ıma margin_nm kaldı ya da daha az: menzil ≤ MAR + margin_nm.
    bool approaching_mar(const Aircraft& own, double margin_nm);

    // Düşmana menzilim range_nm ya da daha az.
    bool range_within(const Aircraft& own, double range_nm);

    // Son atış anı: şimdi atarsam füzem vurduğunda MAR'ımın en fazla margin_nm dışında olurum (F-pole ≤ MAR + pay).
    // Daha geç atarsam vuruştan önce MAR'a girer, füzemi desteklemeden kaçmak zorunda kalırım. Düşman WEZ'imde olmalı.
    bool last_shot_before_mar(const Aircraft& own, double margin_nm);

    // Bana doğru uçan bir düşman füzesi var.
    bool threat_inbound(const Aircraft& own);

    // Bana gelen en acil füzenin kalan süresi limit_s'den kısa.
    bool threat_within_s(const Aircraft& own, double limit_s);

    // Havadaki son füzem kendi arayıcısıyla hedefi görüyor (artık destek gerekmez).
    bool own_missile_pitbull(const Aircraft& own);
}
