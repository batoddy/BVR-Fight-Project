"""İsabet kararı.

OPRF kaynağı (scratchpad/oprf/missile-code.nas): proximity_detection() (~4220), satır 2027 mesajı.
Parametreler: profile.fuze_radius_m, profile.arming_s.

OPRF akışı:
  proximity_detection():
    if life_time > arming_time:
        if crc_range[0] > crc_range[1] and crc_range[0] < 250:      # mesafe artmaya başladı = hedefin yanından geçti
            subframeClosestRangeCoord()                               # en yakın geçiş mesafesi
            explode("Passed target.", ..., crc_closestRange)
  explode():
    if range < reportDist  → isabet ("exploded: X meters from")
    else                   → "missed"
  Satır 2027: "Will explode if target range is increasing and within %d meters of target."
"""

PASS_GATE_M = 250.0   # OPRF: crc_range[0] < 250 — yalnızca hedefin yakınından geçerken tapa bakar


def detonates(missile, target_range_m, previous_range_m):
    """Füze bu adımda patlıyor mu?

    Girdi:
      missile           weapons.missile.Missile: time_s (uçuş süresi), profile
      target_range_m    füze–gerçek hedef mesafesi, bu adım (m)
      previous_range_m  aynı mesafe, önceki adım (m); ilk adımda None
    Çıktı: True → isabet (Missile hedefin health'ini 1 azaltır ve uçuşu bitirir).

    Ne yapmalı: OPRF'teki yakınlık tapasının karşılığı.
    """
    # Tampon henüz dolmadı (OPRF: crc_range[1] == nil → return 0)
    if previous_range_m is None:
        return False

    # Kurulma süresi dolmadan tapa çalışmaz (life_time > arming_time)
    if missile.time_s <= missile.profile.arming_s:
        return False

    # Mesafe hâlâ azalıyorsa en yakın noktaya gelinmedi, beklemeye devam
    if not (target_range_m > previous_range_m and target_range_m < PASS_GATE_M):
        return False

    # En yakın geçiş mesafesi. OPRF bunu iki adım arasında konumları enterpole ederek bulur;
    # elimizde sadece mesafeler var, o yüzden iki ölçümün küçüğünü alıyoruz (en yakın geçişin üst sınırı).
    closest_range_m = min(target_range_m, previous_range_m)

    # explode(): range < reportDist → isabet
    return closest_range_m < missile.profile.fuze_radius_m