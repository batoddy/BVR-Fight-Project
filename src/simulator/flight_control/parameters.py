"""Uçuş kontrolü sabitleri. Her değerin yanında kaynağı ya da "geçici" notu ve seçim yolu vardır.

Ölçüm betikleri: src/simulator/tools/flight_tests.py (uçuş testleri). Ölçüm koşulları TASARIM.md'de.
"""
import math

G0_MPS2 = 9.80665                       # standart yerçekimi (ISO 80000-3)

# =============================================================================
# KOMUT VARSAYILANLARI
# =============================================================================

DEFAULT_G_LIMIT = 6.0                   # g, komutta g_limit yoksa; eski sim DEFAULT_TURN_G
N_MIN = 0.5                             # g, en küçük yük komutu (yatıkken negatif g'ye düşmesin); eski sim N_MIN

# =============================================================================
# 1. İSTENEN YÖN VEKTÖRÜ — irtifa hatasından uçuş yolu açısı
# =============================================================================

K_H = 0.08                              # 1/s, irtifa hatası → dikey hız; eski sim K_H, geçici
VZ_UP_MAX = 60.0                        # m/s, en hızlı tırmanma; eski sim VZ_UP_MAX, geçici
VZ_DOWN_MAX = 120.0                     # m/s, en hızlı alçalma; eski sim VZ_DOWN_MAX, geçici
HEADING_LEAD_MAX_DEG = 90.0             # °, istenen yön iz açısının en fazla bu kadar önüne konur: 180°'de manevra düzlemi yatay kalır (geometri, aşağıda)

# =============================================================================
# 2-3. İŞARET ETME VE MANEVRA DÜZLEMİ — istenen dik ivme
# =============================================================================

K_POINT = 0.5                           # 1/s, hız vektörünün D'ye dönüş hızı = K_POINT·ε; geçici: eski sim K_VZ ve 1/TAU_HEAD (ikisi 0.5), K_H ile ζ=1.25
A_UP_MAX = 1.0 * G0_MPS2                # m/s², işaret etmede en büyük yukarı dik ivme; eski sim A_UP_MAX, geçici
A_DOWN_MAX = (1.0 - N_MIN) * G0_MPS2    # m/s², en büyük aşağı dik ivme: düz kanatla yük N_MIN'in altına inmez (eski sim N_MIN'den), geçici
# ε_geçiş sabit değil: işaret etme ivmesi g_limit'e ulaştığı açıda manevra düzlemine geçilir (aml.py, iki kip süreklidir)

# =============================================================================
# 4. YATIŞ HIZI KOMUTU — CR-145004 bang-bang + doğrusal bölge
# =============================================================================

P_MAX_DPS = 180.0                       # °/s, FBW tam kumanda: roll-rate-norm = p·0.31821 → 1.0 = π rad/s (f16.xml satır ~367)
PDOT_PER_QBAR = 16.1                    # (°/s²)/kPa, ṗ_max = bu · q̄; ölçüm: tam aileron basamağı, 2/5/10 km, M0.5–1.1, oran 16.1–16.6 (en küçüğü)
K_PHI = 2.8                             # 1/s, doğrusal bölge; geçici: yatış hızı döngüsü τ=0.18 s (10 km M0.9 ölçümü) ile ζ≈0.7 → 1/(1.96·τ)

# CR-145004 s.60 "pitch slowly": büyük yatış sürerken yunuslama komutu yarıya (F-4 değerleri, F-16'ya aynen alındı; geçici)
PITCH_SLOW_ROLL_ERROR_DEG = 30.0        # °, yatış hatası bunun üstünde ve
PITCH_SLOW_ROLL_RATE_DPS = 50.0         # °/s, yatış hızı bunun üstündeyse n_kom = (n + n_d)/2

# =============================================================================
# 6. FBW ADAPTÖRÜ — JSBSim F-16
# =============================================================================

AILERON_PER_RAD_S = 1.0 / math.pi       # aileron-cmd-norm / (rad/s); f16.xml roll-rate-norm kazancı 0.31821 ≈ 1/π
KP_G = 0.2                              # elevator / g hatası; geçici: eski sim 0.08'den başlandı, 1→2/4 g basamağında (5/10 km M0.9) 0.2 aşmasız, 0.08 +0.19 g aşma
KI_G = 0.6                              # elevator / (g·s); eski sim KI_G, geçici: aynı basamakta 0.3 yavaş (t90 2.4 s), 0.8–1.0 aşmalı
SATURATED_S = 1.0                       # s, elevator bu kadar tam çekişte kalırsa yük ulaşılamaz sayılır, düzlem ölçülen yükle kurulur; eski sim SATURATED_S, geçici
GRAVITY_COMPENSATION = 1.0              # manevra düzleminde dengelenen yerçekimi oranı; 1: irtifa korunur (alçalmayı C++ irtifa komutu belirler), 0: AML orijinali. Kullanıcı kararı 2026-11-12 (k taraması: TASARIM.md)
