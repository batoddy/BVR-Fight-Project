"""Standart atmosfer: US Standard Atmosphere 1976 (ISA ile 32 km'ye kadar aynı), 0–47 km.

Katmanlar jeopotansiyel irtifada tanımlıdır; geometrik irtifa h = r0·z / (r0 + z) ile çevrilir.
"""
import math

G0 = 9.80665               # m/s², standart yerçekimi
R_AIR = 287.05287          # J/(kg·K), kuru hava gaz sabiti (USSA 1976)
GAMMA = 1.4                # özgül ısılar oranı
R0_M = 6356766.0           # m, USSA 1976 jeopotansiyel irtifa için Dünya yarıçapı

# (taban jeopotansiyel irtifa m, taban sıcaklık K, sıcaklık gradyanı K/m, taban basınç Pa): USSA 1976 Tablo 4
LAYERS = (
    (0.0, 288.15, -0.0065, 101325.0),
    (11000.0, 216.65, 0.0, 22632.06),
    (20000.0, 216.65, 0.001, 5474.889),
    (32000.0, 228.65, 0.0028, 868.0187),
    (47000.0, 270.65, 0.0, 110.9063),
)


def state(alt_m):
    """(sıcaklık K, basınç Pa, yoğunluk kg/m³, ses hızı m/s). Katman dışı irtifa en yakın sınıra kırpılır."""
    z = min(max(alt_m, 0.0), 47000.0)
    h = R0_M * z / (R0_M + z)                                  # jeopotansiyel irtifa
    base_h, base_t, lapse, base_p = next(layer for layer in reversed(LAYERS) if h >= layer[0])
    temperature = base_t + lapse * (h - base_h)
    if lapse == 0.0:
        pressure = base_p * math.exp(-G0 * (h - base_h) / (R_AIR * base_t))
    else:
        pressure = base_p * (temperature / base_t) ** (-G0 / (R_AIR * lapse))
    density = pressure / (R_AIR * temperature)
    return temperature, pressure, density, math.sqrt(GAMMA * R_AIR * temperature)
