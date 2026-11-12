"""Güdüm komutu.

OPRF kaynağı (scratchpad/oprf/missile-code.nas): guide() içinde guidanceLaw "PN" ile yönlendirme sinyali
(track_signal_e / track_signal_h), steering_speed_G() ve limitG(). Parametre: profile.navigation_constant.
"""
import numpy as np

G0 = 9.80665  # Standart yerçekimi ivmesi (m/s²)


def acceleration_command(missile, estimate):
    """Füzenin hedef bilgisine yönelmesi için istenen ivme (ECEF, m/s²).

    Girdi:
      missile   weapons.missile.Missile: position, velocity (ECEF), profile
      estimate  seeker.TargetEstimate: hedefin bilinen/tahmin edilen konumu ve hızı
    Çıktı: 3 elemanlı numpy dizisi. Hız yönündeki bileşeni ve G sınırını missile.py kendisi atar;
           yerçekimi missile.py'de ayrıca uygulanır (gerekirse telafisi bu fonksiyonun işidir).

    Ne yapmalı: OPRF'teki "PN" güdüm yasasının karşılığı.
    """
    if estimate is None or getattr(estimate, "position", None) is None:
        return np.zeros(3, dtype=float)

    r_m = np.asarray(missile.position, dtype=float)
    v_m = np.asarray(missile.velocity, dtype=float)
    r_t = np.asarray(estimate.position, dtype=float)
    v_t = (
        np.asarray(estimate.velocity, dtype=float)
        if getattr(estimate, "velocity", None) is not None
        else np.zeros(3, dtype=float)
    )

    # Hedefe olan bağıl konum (LOS vektörü) ve mesafe
    r_tm = r_t - r_m
    dist_sq = float(np.dot(r_tm, r_tm))
    if dist_sq < 1e-6:
        return np.zeros(3, dtype=float)
    dist = np.sqrt(dist_sq)
    los_unit = r_tm / dist

    # Bağıl hız ve yaklaşma hızı (closing speed)
    # OPRF APN() içinde negatif yaklaşma hızında ters yönlendirmeyi önlemek için mutlak değer kullanılır:
    # math.abs(me.horz_closing_rate_fps) / math.abs(me.vert_closing_rate_fps)
    v_tm = v_t - v_m
    closing_speed = abs(float(-np.dot(v_tm, los_unit)))

    # Bakış hattı açısal hız vektörü (LOS rate, rad/s): Omega = (r_tm x v_tm) / |r_tm|^2
    omega = np.cross(r_tm, v_tm) / dist_sq

    # Navigasyon sabiti (OPRF'teki pro_constant; varsayılan 3.0)
    nav_const = float(getattr(missile.profile, "navigation_constant", 3.0))

    # LOS'a dik PN ivme komutu: a_los_perp = N * |V_c| * (Omega x los_unit)
    a_pn = nav_const * closing_speed * np.cross(omega, los_unit)

    # OPRF'teki toBody = cos(curr_deviation) düzeltmesi:
    # LOS'a dik ivmeyi füze gövde/hız dik eksenine ölçekler (1 / cos(sapma_acisi))
    speed_m = float(np.linalg.norm(v_m))
    if speed_m > 1e-3:
        vel_unit = v_m / speed_m
        to_body = float(np.dot(vel_unit, los_unit))
        # Aşırı sapmalarda sıfıra bölünmeyi ve aşırı kazancı sınırla
        to_body = np.clip(abs(to_body), 0.2, 1.0)
        a_pn = a_pn / to_body

    # Yerçekimi telafisi (OPRF'teki gravComp = -attitudePN karşılığı):
    # ECEF koordinatlarında yerçekimi merkeze doğrudur (-r_m / |r_m| * g0).
    # İrtifa/yunuslama düşüşünü dengelemek için yerel yukarı yönlü (+r_m / |r_m| * g0) ivme eklenir.
    r_m_norm = float(np.linalg.norm(r_m))
    if r_m_norm > 1e-3:
        up_unit = r_m / r_m_norm
        a_grav_comp = G0 * up_unit
    else:
        a_grav_comp = np.zeros(3, dtype=float)

    return a_pn + a_grav_comp