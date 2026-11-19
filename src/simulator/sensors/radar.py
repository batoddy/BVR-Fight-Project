"""Radar: bir uçak diğerini görüyor mu? Kurallar ve kaynaklar: data/aircraft/f16.yaml, protokol §v2.1.

Görme koşulları (hepsi):
  1. menzil ≤ ref_range · (σ_hedef / ref_rcs)^(1/4)            (OPRF apg-68.nas)
  2. hedef burna göre azimut ve yükseliş ±field_of_regard içinde (anten yatışta dengeli kabul)
  3. notch değil: hedef benden alçaktaysa (aşağı bakış, düz yer) |v_hedef · LOS| ≥ notch_speed  (OPRF checkClutter)
Arazi, tarama süresi, RCS'in açıya bağlılığı yok (sadeleştirme).
"""
import math

import numpy as np

from simulator.physics import earth

NM_TO_M = 1852.0
KT_TO_MPS = 1852.0 / 3600.0


class Radar:
    def __init__(self, settings):
        self.ref_range_m = settings["ref_range_nm"] * NM_TO_M
        self.ref_rcs_m2 = settings["ref_rcs_m2"]
        self.max_az_deg = settings["field_of_regard_az_deg"]
        self.max_el_deg = settings["field_of_regard_el_deg"]
        self.notch_speed_mps = settings["notch_speed_kt"] * KT_TO_MPS
        self.realistic = True          # False (world.sensors kapalı): yalnız görüş alanı açısı; menzil ve notch yok

    def detection_range_m(self, rcs_m2):
        return self.ref_range_m * (rcs_m2 / self.ref_rcs_m2) ** 0.25

    def sees(self, own, target):
        """own, target: aircraft.Aircraft. Gerçek konumlardan karar verir; ölçüm gürültüsü yok."""
        los_ned = earth.ecef_to_ned(target.position_ecef() - own.position_ecef(), own.model.lat_deg, own.model.lon_deg)
        distance = float(np.linalg.norm(los_ned))
        azimuth, elevation = self.angles_deg(own, los_ned)
        if abs(azimuth) > self.max_az_deg or abs(elevation) > self.max_el_deg:
            return False
        if not self.realistic:
            return True
        if distance > self.detection_range_m(target.rcs_m2):
            return False
        return not self.in_notch(own, target, los_ned / distance)

    @staticmethod
    def angles_deg(own, los_ned):
        """Görüş hattının burna göre azimut (yatay düzlemde, + sağ) ve yükseliş (+ yukarı) açısı, derece."""
        north, east, down = los_ned
        azimuth = (math.degrees(math.atan2(east, north)) - own.model.yaw_deg + 180.0) % 360.0 - 180.0
        elevation = math.degrees(math.atan2(-down, math.hypot(north, east))) - own.model.pitch_deg
        return azimuth, elevation

    def in_notch(self, own, target, los_unit_ned):
        """OPRF checkClutter: arkada yer varsa (hedef alçakta) ve hedefin görüş hattı boyunca hızı eşikten küçükse."""
        if target.model.alt_m >= own.model.alt_m:
            return False                                                       # yukarı bakış: arka plan gökyüzü
        radial = float(np.dot(target.model.velocity_ned_mps, los_unit_ned))    # yere göre (hava/yer durgun)
        return abs(radial) < self.notch_speed_mps
