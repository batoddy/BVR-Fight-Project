"""Yön/irtifa komutunu yatış ve yük faktörü üzerinden kontrol yüzeylerine çevir.

Kontrol sırası: dikey ivme isteği → yatış → hedef g → elevator komutu.
BVRGym kontrolüne bağlanma nedeni ve önceki ölçümler README'de açıklanır.
"""
import math

import numpy as np

# =============================================================================
# KONTROL SABİTLERİ — mevcut değerler ve fizik adımı
# =============================================================================

G0 = 9.81                 # m/s²
DT_S = 1.0 / 120.0        # s, otopilot her JSBSim adımında (120 Hz) bir kez çağrılır

K_ALT = 0.04              # 1/s², irtifa hatası -> dikey ivme komutu (ω = 0,2 rad/s)
K_VS = 0.4                # 1/s, dikey hız sönümü (2ω)
A_MAX = 3.0               # m/s², irtifa düzeltmesi için en büyük dikey ivme
N_MIN, N_MAX = 0.5, 6.0   # g; uçak yatıkken negatif g'ye düşmesin; üst sınır bizim seçimimiz (F-16: 9)
TURN_G = N_MAX            # g, sert dönüşte hedef yük; yatış: acos(1 / 6) ≈ 80,4°
COS_MIN = 1.0 / N_MAX     # 1/cos sonsuza gitmesin; N_MAX'a karşılık gelen yatışta kesilir
KP_G, KI_G = 0.08, 0.6    # çubuk / g hatası
TAU_HEAD = 2.0            # s, ince ayarda yön hatasını kapatma süresi
BIG_ALT_CHANGE_M = 1.5e3  # m, bunun üstünde BVRGym'in kendi mantığı


# =============================================================================
# BİR UÇAĞIN BVRGYM OTOPİLOTUNA BAĞLAMA
# =============================================================================

def install(autopilot):
    """Bir uçağın AircraftPIDAutopilot nesnesine bu kontrolü bağla (yalnız o uçağı etkiler)."""
    original = autopilot.get_control_input
    autopilot.g_integral = None

    def get_control_input(diff_head, diff_alt):
        return _control(autopilot, original, diff_head, diff_alt)

    autopilot.get_control_input = get_control_input


# =============================================================================
# HER FİZİK ADIMINDA ÇALIŞAN KONTROL AKIŞI
# =============================================================================

def _control(ap, original, diff_head, diff_alt):
    """diff_head: derece (+ sağa dön), diff_alt: m (+ yukarı). Dönüş: aileron, elevator, rudder."""
    aircraft = ap.aircraft

    # Büyük irtifa değişiminde BVRGym'in mevcut tırmanma/dalış kontrolünü kullan.
    if abs(diff_alt) > BIG_ALT_CHANGE_M:
        ap.g_integral = None
        return original(diff_head, diff_alt)

    # Dikey ivme komutu: irtifa hatası ve dikey hız sönümü. Hem yatışta hem çubukta kullanılır.
    v_up = -aircraft.get_v_down()
    a_cmd = float(np.clip(K_ALT * diff_alt - K_VS * v_up, -A_MAX, A_MAX))

    _set_bank(ap, diff_head, a_cmd)
    _set_elevator(ap, a_cmd)

    return ap.aileron_cmd, ap.elevator_cmd, ap.rudder_cmd


# =============================================================================
# YATIŞ — yön hatasından roll hedefi üretme
# =============================================================================

def _set_bank(ap, diff_head, a_cmd):
    """Yön hatası büyükse sert, küçükse koordineli dönüş yatışı uygula."""
    nav = ap.aircraft_navigation
    aircraft = ap.aircraft

    # İki eşik arasında geçiş yaparak sert dönüş/ince ayar seçimini koru.
    if abs(diff_head) >= ap.head_act_space:
        # Seviyeli dönüşte g = 1 / cos(yatış); irtifa düzeltmesini de hesaba kat.
        roll_dir = 1 if diff_head >= 0 else -1
        bank_deg = math.degrees(math.acos(float(np.clip((1.0 + a_cmd / G0) / TURN_G, 0.0, 1.0))))
        ap.set_roll_PID(roll_ref=roll_dir * bank_deg)
        ap.head_act_space = nav.Head_act_space_min
    else:
        ap.head_act_space = nav.Head_act_space_max
        rate_rad_s = math.radians(diff_head) / TAU_HEAD
        bank_deg = math.degrees(math.atan(aircraft.get_true_airspeed() * rate_rad_s / G0))
        ap.set_roll_PID(roll_ref=float(np.clip(bank_deg, -nav.Roll_max, nav.Roll_max)),
                        secondary_pid=abs(diff_head) < 10)


# =============================================================================
# ELEVATOR — yatışta irtifayı koruyacak yük faktörünü izleme
# =============================================================================

def _set_elevator(ap, a_cmd):
    """Mevcut yatıştan hedef g'yi bul ve PI kontrolüyle elevator komutunu güncelle."""
    aircraft = ap.aircraft

    # Hedef g: irtifayı koruyan yük ve dikey ivme düzeltmesi.
    cos_bank = max(math.cos(math.radians(aircraft.get_phi())), COS_MIN)
    n_ref = float(np.clip((1.0 + a_cmd / G0) / cos_bank, N_MIN, N_MAX))
    n = float(aircraft.fdm["forces/load-factor"])
    error = n_ref - n

    if ap.g_integral is None:
        ap.g_integral = -float(aircraft.fdm["fcs/elevator-cmd-norm"])   # o anki çubuktan başla
    ap.g_integral = float(np.clip(ap.g_integral + KI_G * error * DT_S, -1.0, 1.0))
    ap.elevator_cmd = float(np.clip(-(KP_G * error + ap.g_integral), -1.0, 1.0))   # negatif = çek
