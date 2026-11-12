"""NASA AML uçuş kontrolü: yön/irtifa/g komutundan yatış hızı ve yük komutu (TASARIM.md katman 1–5).

Akış (her fizik adımında):
  1. istenen yön vektörü D: komut yönü (iz açısının en fazla 90° önü) + irtifa hatasından uçuş yolu açısı
  2. hata açısı ε = ∠(V̂, D); istenen dik ivme a = V·K_POINT·ε, D'ye doğru (işaret etme)
  3. L = a − g⊥ g_limit'i aşıyorsa manevra düzlemi: dikey bileşen korunur, kalan yük yanala, |L| = g_limit
  4. φ_d = L'nin V̂ çevresindeki açısı → bang-bang + doğrusal yatış hızı komutu
  5. n_d = |L|/g → FBW adaptörü
"""
import math

import numpy as np

from simulator.flight_control import parameters as P
from simulator.flight_control.fbw import FBWAdapter
from simulator.flight_control.geometry import (DOWN, bank_angle, body_axes, direction, level_frame,
                                               perpendicular, unit, wrap_pi)
from simulator.physics.jsbsim_model import PHYSICS_DT_S


class AMLController:
    def __init__(self):
        self.fbw = FBWAdapter()

    def update(self, command, model):
        g_limit = command.g_limit if command.g_limit is not None else P.DEFAULT_G_LIMIT
        velocity = np.array(model.velocity_ned_mps)
        speed = float(np.linalg.norm(velocity))
        v_hat = velocity / speed
        _, right, down = body_axes(*(math.radians(a) for a in (model.roll_deg, model.pitch_deg, model.yaw_deg)))

        track = math.radians(model.flight_path_deg[1])                    # iz açısı χ: hız vektörünün yönü
        target = desired_direction(math.radians(command.heading_deg), track,
                                   desired_gamma(command.altitude_m - model.alt_m, speed))
        g_plane = g_limit                                  # manevra düzleminin kurulduğu yük
        if self.fbw.load_unreachable():                    # F-16 FBW yüksek irtifada g_limit'e çıkamıyor (AML §4.3'teki sorun)
            g_plane = float(np.clip(model.load_factor, 1.0, g_limit))
        lift, maneuver_plane = lift_command(v_hat, speed, target, g_plane, right)

        bank_target, bank_now = bank_angle(lift, v_hat, right), bank_angle(-down, v_hat, right)
        bank_error = wrap_pi(bank_target - bank_now)
        roll_rate = math.radians(model.body_rates_dps[0])
        load_desired = g_limit if maneuver_plane else float(np.clip(np.linalg.norm(lift) / P.G0_MPS2, P.N_MIN, g_limit))
        load_cmd = pitch_slowly(load_desired, model.load_factor, bank_error, roll_rate)
        load_cmd = roll_in_limit(load_cmd, np.linalg.norm(lift) / P.G0_MPS2 * math.cos(bank_target), bank_now)
        self.fbw.update(model, roll_rate_command(bank_error, model.qbar_pa), load_cmd, command.throttle, PHYSICS_DT_S)


# =============================================================================
# 1. İSTENEN YÖN VEKTÖRÜ
# =============================================================================

def desired_gamma(altitude_error_m, speed_mps):
    """İrtifa hatasından istenen uçuş yolu açısı, rad (yukarı +)."""
    climb = float(np.clip(P.K_H * altitude_error_m, -P.VZ_DOWN_MAX, P.VZ_UP_MAX))
    return math.atan2(climb, speed_mps)


def desired_direction(heading_cmd, track, gamma_d):
    """İstenen yön vektörü D. Yön farkı ±HEADING_LEAD_MAX ile kırpılır: dönüş yönü en kısa yoldan,
    180°'lik dönüşte de düzlem yatay kalır (yoksa D ≈ −V̂ olur, düzlemi irtifa gürültüsü seçer: takla)."""
    lead = math.radians(P.HEADING_LEAD_MAX_DEG)
    return direction(track + float(np.clip(wrap_pi(heading_cmd - track), -lead, lead)), gamma_d)


# =============================================================================
# 2–3. İŞARET ETME VE MANEVRA DÜZLEMİ
# =============================================================================

def lift_command(v_hat, speed_mps, target, g_limit, right):
    """İstenen kaldırma ivmesi (NED, m/s²) ve manevra düzlemi kipinde olup olmadığı.

    İşaret etme: a = V·K_POINT·ε ê (ê: D'ye doğru, V̂'ye dik). Yukarı bileşeni A_UP/A_DOWN ile kırpılır
    (irtifa değişimi için ters dönüp çekmesin). L = a − g⊥. |L| g_limit'i aşarsa manevra düzlemi
    (AML: büyük hatada azami yük): yerçekiminin GRAVITY_COMPENSATION kadarı dengelenir (1: irtifa korunur,
    0: AML orijinali, kaldırma tamamen D'ye doğru), kalan yük yanal bileşene verilir.
    """
    error_angle = math.acos(float(np.clip(np.dot(v_hat, target), -1.0, 1.0)))
    accel = speed_mps * P.K_POINT * error_angle * unit(perpendicular(target, v_hat))
    down_level, _ = level_frame(v_hat, right)
    up = -down_level                                       # yatışsız kaldırma yönü
    lateral = accel - np.dot(accel, up) * up
    accel_up = float(np.clip(np.dot(accel, up), -P.A_DOWN_MAX, P.A_UP_MAX))   # irtifa değişimi için dikey ivme
    gravity_up = P.G0_MPS2 * np.dot(DOWN, down_level)                          # g·cosγ
    side_lift = float(np.linalg.norm(lateral))
    limit = g_limit * P.G0_MPS2
    if math.hypot(accel_up + gravity_up, side_lift) <= limit:                  # işaret etme
        return (accel_up + gravity_up) * up + side_lift * unit(lateral), False
    up_lift = min(accel_up + P.GRAVITY_COMPENSATION * gravity_up, limit)      # manevra düzlemi
    side_lift = math.sqrt(max(limit * limit - up_lift * up_lift, 0.0))
    return up_lift * up + side_lift * unit(lateral), True


# =============================================================================
# 4. YATIŞ HIZI KOMUTU (CR-145004)
# =============================================================================

def roll_rate_command(bank_error, qbar_pa):
    """p_kom = sign(φe)·min(p_max, √(2·ṗ_max·|φe|), Kφ·|φe|), rad/s. ṗ_max dinamik basınçla ölçeklenir."""
    pdot_max = math.radians(P.PDOT_PER_QBAR * qbar_pa / 1000.0)
    size = abs(bank_error)
    return math.copysign(min(math.radians(P.P_MAX_DPS), math.sqrt(2.0 * pdot_max * size), P.K_PHI * size), bank_error)


# =============================================================================
# 5. YÜK KOMUTU
# =============================================================================

def roll_in_limit(load_cmd, vertical_target, bank_now):
    """Yatış oturmadan çekilen yük fazla dikey kaldırma üretir (out başında tırmanma).
    Yükün dikey bileşeni istenenden fazla olmasın: n·cos(φ_şimdi) ≤ n_d·cos(φ_d) (uçuş mekaniği, ayar sayısı yok).
    Ters yatışta (cos ≤ 0) sınır uygulanmaz."""
    cos_now = math.cos(bank_now)
    if vertical_target <= 0.0 or cos_now <= 0.0:
        return load_cmd
    return max(min(load_cmd, vertical_target / cos_now), P.N_MIN)


def pitch_slowly(load_desired, load_now, bank_error, roll_rate):
    """CR-145004 s.60: büyük yatış sürerken yunuslama hedefi şimdiki ile istenenin ortası."""
    if (abs(bank_error) > math.radians(P.PITCH_SLOW_ROLL_ERROR_DEG)
            and abs(roll_rate) > math.radians(P.PITCH_SLOW_ROLL_RATE_DPS)):
        return max(0.5 * (load_now + load_desired), P.N_MIN)
    return load_desired
