"""Füze: 3-DOF nokta kütle (ECEF), itki, sürükleme, dönüş kaybı, yerçekimi; atış ve uçuş sonu.

Hedef bilgisi (seeker), güdüm (guidance) ve isabet (fuze) ayrı modüllerdedir.
Her fizik adımında: hedef bilgisi → güdüm ivmesi (G sınırlı) → kuvvetler → hareket → isabet / bitiş.
Varsayım: ECEF'te yalnız yerçekimi; Dünya dönüşünün Coriolis/merkezkaç etkisi ihmal (≤0.2 m/s²). Hava Dünya'ya göre durgun.
"""
import math

import numpy as np

from simulator.physics import atmosphere, earth
from simulator.weapons import fuze, guidance
from simulator.weapons.seeker import Seeker

G0 = 9.80665
METERS_PER_FOOT = 0.3048
BLEED_ALT_FT = 32800.0                       # OPRF energyBleed tablosunun üst irtifası
BLEED_SEA_LEVEL = 750.0 / 7.0 - 950.0 / 22.0     # m/s², 25 g'de ek hız kaybı, deniz seviyesi (OPRF bleed0at25g − bleed0at0g)
BLEED_HIGH = 240.0 / 3.5 - 330.0 / 15.0          # m/s², aynı, 32800 ft (OPRF bleed32800at25g − bleed32800at0g)
BLEED_G = 25.0                               # OPRF tablosunun g noktası


def zero_lift_drag_coefficient(mach, base):
    """OPRF missile-code.nas drag(): ses altı / ses civarı / ses üstü parçalı Cd0(M) × 5 × drag-coeff."""
    if mach < 0.7:
        shape = 0.0125 * mach + 0.20
    elif mach < 1.2:
        shape = 0.3742 * mach ** 2 - 0.252 * mach + 0.0021 + 0.2
    else:
        shape = 0.2965 * mach ** -1.1506 + 0.2
    return shape * 5.0 * base


def turn_bleed(g_lateral, alt_m):
    """Dönüşün ek hız kaybı (m/s², pozitif): OPRF energyBleed() tablosu, g ve irtifada doğrusal (irtifada dış değer)."""
    alt_ft = alt_m / METERS_PER_FOOT
    at_25g = BLEED_SEA_LEVEL + alt_ft / BLEED_ALT_FT * (BLEED_HIGH - BLEED_SEA_LEVEL)
    return max(g_lateral / BLEED_G * at_25g, 0.0)


class Missile:
    def __init__(self, profile, slot):
        self.profile = profile
        self.slot = slot
        self.state = "ready"            # ready / in_flight / ended
        self.outcome = None             # hit / lost
        self.target_key = None
        self.seeker = Seeker(profile)
        self.time_s = 0.0               # uçuş süresi
        self.position = np.zeros(3)     # ECEF, m
        self.velocity = np.zeros(3)     # ECEF, m/s
        self.mass_kg = profile.mass_launch_kg
        self.lateral_g = 0.0            # son adımdaki güdüm yükü (kayıt için)
        self.target_range_m = None
        self.area_m2 = math.pi * profile.diameter_m ** 2 / 4.0
        impulse = profile.thrust_n * profile.burn_s + profile.sustain_thrust_n * profile.sustain_s
        self.burn_rate_per_impulse = profile.propellant_kg / impulse   # kg/(N·s): itici, verilen itkiyle orantılı tükenir

    # =========================================================================
    # ATIŞ VE ADIM
    # =========================================================================

    def launch(self, launcher_model, target_key):
        """Atan uçağın konumu ve hızıyla başlat."""
        self.position = earth.geodetic_to_ecef(launcher_model.lat_deg, launcher_model.lon_deg, launcher_model.alt_m)
        self.velocity = earth.ned_to_ecef(launcher_model.velocity_ned_mps, launcher_model.lat_deg, launcher_model.lon_deg)
        self.target_key = target_key
        self.state = "in_flight"

    def step(self, dt, launcher, target):
        """Bir fizik adımı. launcher/target: aircraft.Aircraft."""
        lat, lon, alt = earth.ecef_to_geodetic(self.position)
        _, _, density, sound_mps = atmosphere.state(alt)
        speed = float(np.linalg.norm(self.velocity))
        v_hat = self.velocity / speed
        mach = speed / sound_mps

        command = np.zeros(3)
        estimate = self.seeker.update(self, launcher, target)
        if estimate is not None and mach >= self.profile.min_guide_mach:
            command = guidance.acceleration_command(self, estimate)
        command = command - np.dot(command, v_hat) * v_hat                  # yalnız yön değiştiren bileşen
        limit = self.profile.max_g * G0
        if np.linalg.norm(command) > limit:
            command *= limit / np.linalg.norm(command)
        self.lateral_g = float(np.linalg.norm(command)) / G0

        thrust = self.thrust_n()
        cd = zero_lift_drag_coefficient(mach, self.profile.drag_coeff_base)
        if thrust > 0.0:
            cd *= self.profile.plume_drag_factor
        drag = 0.5 * density * speed ** 2 * self.area_m2 * cd
        along = (thrust - drag) / self.mass_kg - turn_bleed(self.lateral_g, alt)
        acceleration = along * v_hat + command + earth.gravity_ecef(self.position)

        self.velocity = self.velocity + acceleration * dt                    # yarı örtük Euler
        self.position = self.position + self.velocity * dt
        self.mass_kg -= thrust * dt * self.burn_rate_per_impulse
        self.time_s += dt
        self._check_end(target, mach)

    def thrust_n(self):
        p = self.profile
        if self.time_s < p.burn_s:
            return p.thrust_n
        if self.time_s < p.burn_s + p.sustain_s:
            return p.sustain_thrust_n
        return 0.0

    def _check_end(self, target, mach):
        previous = self.target_range_m
        self.target_range_m = float(np.linalg.norm(self.position - target.position_ecef()))
        if fuze.detonates(self, self.target_range_m, previous):
            self.state, self.outcome = "ended", "hit"
            target.health -= 1                                                # eski sim davranışı
            return
        _, _, alt = earth.ecef_to_geodetic(self.position)
        burned_out = self.time_s >= self.profile.burn_s + self.profile.sustain_s
        if (self.time_s >= self.profile.self_destruct_s or alt <= 0.0
                or (burned_out and mach < self.profile.min_guide_mach)):
            self.state, self.outcome = "ended", "lost"

    # =========================================================================
    # ÖLÇÜM
    # =========================================================================

    def kinematics(self):
        """C++'a giden alanlar (uçaktakiyle aynı adlar); roll yok."""
        lat, lon, alt = earth.ecef_to_geodetic(self.position)
        north, east, down = earth.ecef_to_ned(self.velocity, lat, lon)
        speed = float(np.linalg.norm(self.velocity))
        return dict(lat_deg=lat, lon_deg=lon, alt_msl_m=alt,
                    yaw_deg=math.degrees(math.atan2(east, north)) % 360.0,
                    pitch_deg=math.degrees(math.atan2(-down, math.hypot(north, east))), roll_deg=0.0,
                    tas_mps=speed, mach=speed / atmosphere.state(alt)[3],
                    velocity_ned_mps=[float(north), float(east), float(down)])
