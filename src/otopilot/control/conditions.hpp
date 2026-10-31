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
// koşullar aynı kalır. Yalnız picture ve füze durumunu okur; hesap yapmaz.
namespace condition
{
    /// Düşmana menzilim MAR'ımın içinde: şimdi out yapmazsam kaçamayabilirim.
    bool in_mar(const Aircraft& own);

    /// Ben düşmanın MAR'ının içindeyim: düşmanın kendi hesabına göre out yapması gereken mesafe.
    bool inside_enemy_mar(const Aircraft& own, const Aircraft& enemy);

    /// Düşman benim WEZ'imde.
    bool enemy_in_my_wez(const Aircraft& own);

    /// Düşman benim NEZ'imde: şimdi atarsam out yapsa da kaçamaz.
    bool enemy_in_my_nez(const Aircraft& own);

    /// Düşmanın bakışı: düşmanın burnu bana göre nasıl (picture.aspect_deg).
    Aspect enemy_aspect(const Aircraft& own);

    /// Benim bakışım: benim burnum düşmana göre nasıl (picture.ata_deg).
    Aspect my_aspect(const Aircraft& own);

    /// Bana doğru uçan bir düşman füzesi var.
    bool threat_inbound(const Aircraft& own);

    /// fire_order'ıncı attığım füze (1: ilk, 2: ikinci, ...) kendi arayıcısıyla hedefi görüyor (artık destek gerekmez).
    /// O sırada atılmış füze yoksa false.
    bool own_missile_pitbull(const Aircraft& own, int fire_order);
}
