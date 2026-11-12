"""Hedef bilgisi: füzenin bu adımda hedef hakkında ne bildiği.

Kurallar: weapons/TASARIM.md "Güdüm ve arayıcı" tablosu. OPRF kaynakları (scratchpad/oprf/):
  missile-code.nas  guide() "sample" bölümü (~3220), checkForGuidance() (~3440)
  station-manager.nas  AIM-120 uçuş içi fonksiyonu (~159)

OPRF AIM-120 akışı (bu dosyanın karşılığı):
  guidance = "sample"  → atan uçağın radar izi (blep) üzerinden hedef konumu
      yeni iz varsa         → güncel konum                       (bizde "datalink")
      iz yenilenmiyorsa     → son iz + hız * geçen süre           (bizde "inertial")
      hiç iz yoksa          → me.free = 1, güdüm biter            (bizde "none")
  station-manager: dist_m < 10 NM ve guidance == "sample" → guidance = "radar" (pitbull),
      abort_midflight_function = 1 → bir daha "sample"a dönülmez.
  guidance = "radar"   → checkForGuidance(): hedef arayıcı menzili / FOV dışına çıkarsa
      AIM-120 reaquire = true (f16-payload.xml) → fovLost = 1, guiding = 0: güdüm durur, hedef
      görüş alanına dönerse yeniden yakalanır (bizde "none" ↔ "seeker").
"""
from dataclasses import dataclass

import numpy as np

MODES = ("none", "datalink", "inertial", "seeker")   # kayıttaki guidance_mode değerleri; "seeker"e ilk geçiş = pitbull olayı

NM2M = 1852.0


@dataclass
class TargetEstimate:
    """Füzenin hedef hakkındaki bilgisi (ECEF, m ve m/s). Gerçek konum olmak zorunda değil (tahmin olabilir)."""
    position: np.ndarray
    velocity: np.ndarray


# --- aircraft.Aircraft erişimi ----------------------------------------------------------
# Uçağın ECEF durumu yalnızca bu üç fonksiyondan okunuyor. Aircraft sınıfının alan adları
# farklıysa (ya da dönüşüm missile.py'deki ecef yardımcılarıyla yapılıyorsa) sadece bunları değiştir.

def _ac_position(ac) -> np.ndarray:
    """Uçağın ECEF konumu (m)."""
    return np.asarray(ac.position_ecef(), dtype=float)


def _ac_velocity(ac) -> np.ndarray:
    """Uçağın ECEF hızı (m/s)."""
    return np.asarray(ac.velocity_ecef(), dtype=float)


def _ac_forward(ac) -> np.ndarray:
    """Uçağın burun yönü, ECEF birim vektör. Burun vektörü yoksa hız yönü kullanılır (AoA ihmal)."""
    fwd = getattr(ac, "forward_ecef", None)
    v = np.asarray(fwd if fwd is not None else _ac_velocity(ac), dtype=float)
    return v / np.linalg.norm(v)


def _angle_deg(a: np.ndarray, b: np.ndarray) -> float:
    """İki vektör arasındaki açı (derece). OPRF FOV_check()'teki angleBetweenVectors karşılığı."""
    na, nb = np.linalg.norm(a), np.linalg.norm(b)
    if na == 0.0 or nb == 0.0:
        return 180.0
    return float(np.degrees(np.arccos(np.clip(np.dot(a, b) / (na * nb), -1.0, 1.0))))


class Seeker:
    def __init__(self, profile):
        self.profile = profile      # MissileProfile: launcher_field_of_regard_deg, pitbull_nm, seeker_field_deg, seeker_range_nm
        self.mode = "none"          # MODES'tan biri; Missile her adımdan sonra okur

        self._track = None          # son veri bağı paketi (konum, hız, zaman) — OPRF'teki son blep
        self._pitbull = False       # "sample" → "radar" geçişi yapıldı mı (geri dönüş yok)
        self._lost = False          # OPRF me.free: güdüm kalıcı olarak bitti

    @property
    def is_pitbull(self):
        """Füze kendi arayıcısına geçti mi (geri dönüş yok); kayıt için."""
        return self._pitbull

    def update(self, missile, launcher, target):
        """Bu adımda füzenin kullanacağı hedef bilgisini döndür ve self.mode'u güncelle.

        Girdi:
          missile   weapons.missile.Missile: position, velocity (ECEF), time_s, profile
          launcher  aircraft.Aircraft: atan uçak (model: konum, yönelim; ecef yardımcıları missile.py'de)
          target    aircraft.Aircraft: gerçek hedef
        Çıktı: TargetEstimate ya da None (bilgi yok → güdüm yok, füze düz uçar).

        Ne yapmalı (OPRF AIM-120 davranışı):
          - atan uçak hedefi radar görüş alanında tutuyorsa güncel bilgi (mode "datalink")
          - tutmuyorsa son bilgiden tahmin (mode "inertial")
          - hedefe kalan mesafe pitbull eşiğinin altına inince kendi arayıcısı (mode "seeker")
          - hiç bilgi yoksa None (mode "none")
        """
        if self._lost:
            self.mode = "none"
            return None

        m_pos = np.asarray(missile.position, dtype=float)

        if not self._pitbull:
            est = self._sample(missile.time_s, launcher, target)
            if est is None:
                self.mode = "none"
                return None
            # station-manager AIM-120: struct.dist_m*M2NM < 10 and guidance == "sample".
            # dist_m, füzenin TAHMİNİ hedef konumuna uzaklığı (dist_curr_direct → t_coord_sampled).
            if np.linalg.norm(est.position - m_pos) >= self.profile.pitbull_nm * NM2M:
                return est
            self._pitbull = True    # pitbull: bu adımdan itibaren kendi radarı

        return self._active(missile, target)

    # --- guidance == "sample" (guide() ~3220) ----------------------------------------------

    def _sample(self, time_s, launcher, target):
        tgt_pos = _ac_position(target)
        tgt_vel = _ac_velocity(target)

        # Yeni blep: atan uçağın radarı hedefi görüyor → güncel konum (t_coord_sampled = t_coord).
        # İlk çağrı: atış anındaki devir (release() içinde t_coord = Tgt.get_Coord(), newTargetAssigned).
        if self._track is None or self._launcher_tracks(launcher, tgt_pos):
            self._track = (tgt_pos.copy(), tgt_vel.copy(), time_s)
            self.mode = "datalink"
            return TargetEstimate(tgt_pos.copy(), tgt_vel.copy())

        # Aynı blep: dead-reckon. blep_coord + ecefVel * (life_time - blepIDtime)
        p0, v0, t0 = self._track
        p = p0 + v0 * (time_s - t0)
        # OPRF irtifayı dead-reckon etmez (set_alt(blep_coord.alt()), dünya eğriliği yüzünden).
        # Karşılığı: yer merkezine uzaklığı son izdeki değerde tut.
        p *= np.linalg.norm(p0) / np.linalg.norm(p)
        self.mode = "inertial"
        return TargetEstimate(p, v0.copy())

    def _launcher_tracks(self, launcher, tgt_pos) -> bool:
        """Hedef atan uçağın radar görüş alanında mı (OPRF: FCS-field-deg / 2 = fcs_fov)."""
        half_fov = self.profile.launcher_field_of_regard_deg   # yaml değeri zaten yarım açı (OPRF apg-68.nas ±60°)
        los = tgt_pos - _ac_position(launcher)
        return _angle_deg(_ac_forward(launcher), los) <= half_fov

    # --- guidance == "radar" (checkForGuidance() ~3440) -----------------------------------

    def _active(self, missile, target):
        tgt_pos = _ac_position(target)
        los = tgt_pos - np.asarray(missile.position, dtype=float)

        in_range = np.linalg.norm(los) <= self.profile.seeker_range_nm * NM2M      # detect_range_curr_nm
        # FOV_check(me.hdg, me.pitch, ...): füzede AoA yok, burun = hız vektörü. seeker-field-deg / 2 = max_seeker_dev
        in_fov = _angle_deg(np.asarray(missile.velocity, dtype=float), los) <= self.profile.seeker_field_deg / 2.0

        if not (in_range and in_fov):
            # reaquire = true: güdüm bu adım yok, hedef görüş alanına dönerse yeniden yakalanır
            self.mode = "none"
            return None

        self.mode = "seeker"
        return TargetEstimate(tgt_pos.copy(), _ac_velocity(target).copy())