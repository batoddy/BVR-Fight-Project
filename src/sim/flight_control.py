"""Yön/irtifa komutunu yatış ve yük faktörü üzerinden kontrol yüzeylerine çevir.

Kontrol sırası: irtifa hatası → istenen dikey hız → istenen dikey ivme → yatış → hedef g → elevator.
Dönüşte çekilecek en büyük yük (g sınırı) C++ komutundan gelir; gelmezse DEFAULT_TURN_G.
  Sert g (ör. 6-7): out / abort. Yatarken hemen çeker; alçalma istenirse burun aşağı döner.
  Yumuşak g (ör. 3-4): crank, beam, recommit. Daha az yatış, hız ve irtifa korunur.
BVRGym kontrolüne bağlanma nedeni ve önceki ölçümler README'de açıklanır.
"""
import math

import numpy as np

# =============================================================================
# AÇ / KAPA
# 1: bu dosyadaki kontrol uçurur (yatış + g kontrolü).
# 0: bağlanmaz; BVRGym'in kendi AircraftPIDAutopilot'u uçurur (g sınırı yok sayılır).
# İki durumda da yön/irtifa/gaz hedefleri C++'tan gelir; değişen yalnız hedefi uygulayan kontrol.
# =============================================================================

FLIGHT_CONTROL_ENABLED = 1

# =============================================================================
# KONTROL SABİTLERİ — mevcut değerler ve fizik adımı
# =============================================================================

G0 = 9.81                 # m/s²
# Kontrol her JSBSim fizik adımında bir kez çağrılır; adım süresi (dt) modelden okunur (fdm.get_delta_t()).

# Dikey kanal: irtifa hatası -> istenen dikey hız -> istenen dikey ivme.
# İki çarpanla ω = sqrt(K_H · K_VZ) = 0,2 rad/s, sönüm 1,25 (aşmasız yaklaşma).
K_H = 0.08                # 1/s, irtifa hatası -> dikey hız hedefi (1000 m -> 80 m/s)
K_VZ = 0.5                # 1/s, dikey hız hatası -> dikey ivme komutu
VZ_UP_MAX = 60.0          # m/s, en hızlı tırmanma
VZ_DOWN_MAX = 120.0       # m/s, en hızlı alçalma (out'ta alçalarak hız kazanmak için daha büyük)
A_UP_MAX = 1.0 * G0       # m/s², en büyük yukarı dikey ivme komutu
A_DOWN_MAX = 1.0 * G0     # m/s², en büyük aşağı dikey ivme komutu (sert dönüşte yatış 90°'ye kadar)
A_UP_HARD = 0.3 * G0      # m/s², sert dönüşte yatışı küçültmek için kullanılan en büyük yukarı ivme:
                          # uçak turn_g'ye çıkamayıp alçalırsa dönüşten çalmaz, irtifa kaybını kabul eder

# Yük faktörü.
DEFAULT_TURN_G = 6.0      # g, komutta g_limit yoksa dönüşte çekilecek en büyük yük
N_MIN = 0.5               # g; uçak yatıkken negatif g'ye düşmesin
ENTRY_G = 1.0             # g, sert dönüşe girerken yatış oturana kadar izin verilen ek dikey yük
KP_G, KI_G = 0.08, 0.6    # çubuk / g hatası
SATURATED_S = 1.0         # s, elevator bu kadar tam çekişte kalırsa uçak turn_g'ye ulaşamıyor sayılır
                          # (F-16 uçuş kontrol bilgisayarı yavaş hızda yükü sınırlar); yatış ulaşılan g'ye göre kurulur

TAU_HEAD = 2.0            # s, ince ayarda yön hatasını kapatma süresi


# =============================================================================
# BİR UÇAĞIN BVRGYM OTOPİLOTUNA BAĞLAMA
# =============================================================================

class FlightControl:
    """Bir uçağın kontrol durumu. Bizim alanlarımız burada durur; BVRGym nesnesine alan eklenmez.

    BVRGym'e tek müdahale: AircraftPIDAutopilot.get_control_input bu nesnenin kontrolüyle değiştirilir.
    """

    def __init__(self, autopilot):
        self.autopilot = autopilot
        self.g_limit = None        # g; Simulation.step her adım komuttan yazar; None: DEFAULT_TURN_G
        self.g_integral = None     # elevator PI integrali; ilk çağrıda o anki çubuktan başlar
        self.saturated_s = 0.0     # s, elevator'ın kesintisiz tam çekişte kaldığı süre


def install(autopilot):
    """Bir uçağın AircraftPIDAutopilot nesnesine bu kontrolü bağla (yalnız o uçağı etkiler); durumunu döndür."""
    control = FlightControl(autopilot)
    if FLIGHT_CONTROL_ENABLED:
        autopilot.get_control_input = lambda diff_head, diff_alt: _control(control, diff_head, diff_alt)
    # 0 ise BVRGym'in orijinal get_control_input'u yerinde kalır; g_limit yok sayılır.
    return control


# =============================================================================
# HER FİZİK ADIMINDA ÇALIŞAN KONTROL AKIŞI
# =============================================================================

def _control(control, diff_head, diff_alt):
    """diff_head: derece (+ sağa dön), diff_alt: m (+ yukarı). Dönüş: aileron, elevator, rudder."""
    ap = control.autopilot
    turn_g = control.g_limit if control.g_limit is not None else DEFAULT_TURN_G

    a_cmd = _vertical_accel(ap, diff_alt)
    hard_turn = _set_bank(control, diff_head, a_cmd, turn_g)
    _set_elevator(control, a_cmd, hard_turn, turn_g)

    return ap.aileron_cmd, ap.elevator_cmd, ap.rudder_cmd


# =============================================================================
# DİKEY KANAL — irtifa hatasından istenen dikey ivme
# =============================================================================

def _vertical_accel(ap, diff_alt):
    """İrtifa hatasını dikey hız hedefine, dikey hız hatasını dikey ivmeye çevir (m/s²).

    Büyük irtifa değişimi de bu yoldan yapılır: dikey hız VZ_UP_MAX / VZ_DOWN_MAX ile sınırlı.
    Dönüşte istenmeyen tırmanma (dikey hız) da burada A_DOWN_MAX gücüyle durdurulur.
    """
    v_up = -ap.aircraft.get_v_down()
    vz_ref = float(np.clip(K_H * diff_alt, -VZ_DOWN_MAX, VZ_UP_MAX))
    return float(np.clip(K_VZ * (vz_ref - v_up), -A_DOWN_MAX, A_UP_MAX))


# =============================================================================
# YATIŞ — yön hatasından roll hedefi üretme
# =============================================================================

def _set_bank(control, diff_head, a_cmd, turn_g):
    """Yön hatası büyükse sert, küçükse koordineli dönüş yatışı uygula. Sert dönüşse True."""
    ap = control.autopilot
    nav = ap.aircraft_navigation
    aircraft = ap.aircraft

    # İki eşik arasında geçiş yaparak sert dönüş/ince ayar seçimini koru.
    if abs(diff_head) >= ap.head_act_space:
        # Yük turn_g iken düşey bileşen: turn_g · cos(yatış) = 1 + a/g.
        # a = 0 seviyeli dönüş (6 g: 80,4°). a < 0 alçalma: yatış 90°'ye yaklaşır, burun aşağı döner.
        # Yukarı düzeltme A_UP_HARD ile sınırlı: out'ta öncelik dönüş, irtifa sonra toplanır.
        # Uçak turn_g'ye ulaşamıyorsa (elevator SATURATED_S'den uzun tam çekişte) yatış ulaşılan
        # g'ye göre kurulur; yoksa 80° yatışta düşük g ile sürekli alçalırdı.
        roll_dir = 1 if diff_head >= 0 else -1
        a_bank = min(a_cmd, A_UP_HARD)
        bank_g = turn_g
        if control.saturated_s > SATURATED_S:
            bank_g = float(np.clip(aircraft.fdm["forces/load-factor"], 1.5, turn_g))
        cos_bank = float(np.clip((1.0 + a_bank / G0) / bank_g, 0.0, 1.0))
        ap.set_roll_PID(roll_ref=roll_dir * math.degrees(math.acos(cos_bank)))
        ap.head_act_space = nav.Head_act_space_min
        return True

    # Koordineli dönüş: yön hatasını TAU_HEAD'de kapatacak dönüş hızı; yatış g sınırını aşmaz.
    ap.head_act_space = nav.Head_act_space_max
    rate_rad_s = math.radians(diff_head) / TAU_HEAD
    bank_deg = math.degrees(math.atan(aircraft.get_true_airspeed() * rate_rad_s / G0))
    bank_max = min(nav.Roll_max, math.degrees(math.acos(1.0 / turn_g)))
    ap.set_roll_PID(roll_ref=float(np.clip(bank_deg, -bank_max, bank_max)),
                    secondary_pid=abs(diff_head) < 10)
    return False


# =============================================================================
# ELEVATOR — hedef yük faktörünü izleme
# =============================================================================

def _set_elevator(control, a_cmd, hard_turn, turn_g):
    """Mevcut yatıştan hedef g'yi bul ve PI kontrolüyle elevator komutunu güncelle."""
    ap = control.autopilot
    aircraft = ap.aircraft
    dt_s = aircraft.fdm.get_delta_t()   # s, bir JSBSim fizik adımı

    # Düşey bileşen: n · cos(yatış) = 1 + a/g. 1/cos, turn_g'ye karşılık gelen yatışta kesilir.
    cos_bank = max(math.cos(math.radians(aircraft.get_phi())), 1.0 / turn_g)

    if hard_turn:
        # Sert dönüşte yatışın oturmasını beklemeden çek: yük turn_g'ye kadar çıkar, ama düşey
        # bileşeni istenen dikey ivmeden en fazla ENTRY_G fazla olur. Yatış küçükken uçak kısa
        # süre tırmanır; dikey hız büyüdükçe a_cmd azalır ve tırmanmayı kendisi keser.
        n_ref = (1.0 + a_cmd / G0 + ENTRY_G) / cos_bank
    else:
        n_ref = (1.0 + a_cmd / G0) / cos_bank
    n_ref = float(np.clip(n_ref, N_MIN, turn_g))

    n = float(aircraft.fdm["forces/load-factor"])
    error = n_ref - n

    if control.g_integral is None:
        control.g_integral = -float(aircraft.fdm["fcs/elevator-cmd-norm"])   # o anki çubuktan başla
    control.g_integral = float(np.clip(control.g_integral + KI_G * error * dt_s, -1.0, 1.0))
    ap.elevator_cmd = float(np.clip(-(KP_G * error + control.g_integral), -1.0, 1.0))   # negatif = çek

    # Tam çekişte geçen süre: _set_bank uçağın turn_g'ye ulaşıp ulaşamadığını buradan anlar.
    if ap.elevator_cmd <= -0.99 and hard_turn:
        control.saturated_s += dt_s
    else:
        control.saturated_s = 0.0
