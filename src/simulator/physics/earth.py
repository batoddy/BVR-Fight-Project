"""WGS84 Dünya modeli: jeodezik ↔ ECEF dönüşümü, yerel NED eksenleri, yerçekimi.

Kaynak: NIMA TR8350.2 (WGS84) sabitleri. ECEF: Dünya merkezli, Dünya'ya sabit eksenler (m).
"""
import math

import numpy as np

A_M = 6378137.0                    # WGS84 büyük yarı eksen
F = 1.0 / 298.257223563            # WGS84 basıklık
E2 = F * (2.0 - F)                 # birinci dışmerkezlik karesi
B_M = A_M * (1.0 - F)              # küçük yarı eksen
GM = 3.986004418e14                # m³/s², WGS84 yerçekimi sabiti


def geodetic_to_ecef(lat_deg, lon_deg, alt_m):
    lat, lon = math.radians(lat_deg), math.radians(lon_deg)
    n = A_M / math.sqrt(1.0 - E2 * math.sin(lat) ** 2)   # birinci düşey eğrilik yarıçapı
    return np.array([(n + alt_m) * math.cos(lat) * math.cos(lon),
                     (n + alt_m) * math.cos(lat) * math.sin(lon),
                     (n * (1.0 - E2) + alt_m) * math.sin(lat)])


def ecef_to_geodetic(position):
    """ECEF (m) → (enlem °, boylam °, elipsoid yüksekliği m). Bowring başlangıcı + yineleme; cm altı doğruluk."""
    x, y, z = position
    lon = math.atan2(y, x)
    p = math.hypot(x, y)
    lat = math.atan2(z, p * (1.0 - E2))                  # ilk tahmin
    for _ in range(5):
        n = A_M / math.sqrt(1.0 - E2 * math.sin(lat) ** 2)
        alt = p / math.cos(lat) - n
        lat = math.atan2(z, p * (1.0 - E2 * n / (n + alt)))
    n = A_M / math.sqrt(1.0 - E2 * math.sin(lat) ** 2)
    alt = p / math.cos(lat) - n
    return math.degrees(lat), math.degrees(lon), alt


def ned_axes(lat_deg, lon_deg):
    """Yerel kuzey, doğu, aşağı birim vektörleri (ECEF'te), 3×3 satırlar halinde."""
    lat, lon = math.radians(lat_deg), math.radians(lon_deg)
    north = np.array([-math.sin(lat) * math.cos(lon), -math.sin(lat) * math.sin(lon), math.cos(lat)])
    east = np.array([-math.sin(lon), math.cos(lon), 0.0])
    down = np.array([-math.cos(lat) * math.cos(lon), -math.cos(lat) * math.sin(lon), -math.sin(lat)])
    return np.array([north, east, down])


def ned_to_ecef(vector_ned, lat_deg, lon_deg):
    return ned_axes(lat_deg, lon_deg).T @ np.asarray(vector_ned, dtype=float)


def ecef_to_ned(vector_ecef, lat_deg, lon_deg):
    return ned_axes(lat_deg, lon_deg) @ np.asarray(vector_ecef, dtype=float)


def gravity_ecef(position):
    """Nokta kütle yerçekimi ivmesi (m/s²), Dünya merkezine doğru. Dönen eksen etkileri (Coriolis) ihmal."""
    r = np.linalg.norm(position)
    return -GM * position / r ** 3
