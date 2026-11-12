"""JSBSim F-16: yükleme, başlangıç koşulu, kumanda yazma ve SI birimli durum okuma.

Uçuş kontrolü yoktur; kumandayı flight_control yazar. Fizik yalnız run() ile ilerler.
"""
import math
from pathlib import Path

import jsbsim

DATA_ROOT = Path(__file__).resolve().parents[1] / "jsbsim_data"   # f16.xml ve motor dosyaları (BVRGym kopyası)
MODEL_NAME = "f16"
PHYSICS_DT_S = 1.0 / 120.0   # BVRGym scripts/f16_test.xml dt="0.00833333"

METERS_PER_FOOT = 0.3048
G0_MPS2 = 9.80665            # standart yerçekimi (ISO 80000-3)

jsbsim.FGJSBBase().debug_lvl = 0   # her model yüklemesinde basılan JSBSim başlığını kapat


class JSBSimModel:
    def __init__(self):
        self.fdm = jsbsim.FGFDMExec(str(DATA_ROOT), None)   # kayıt property kataloğunu buradan okur
        if not self.fdm.load_model(MODEL_NAME):
            raise RuntimeError(f"JSBSim modeli yüklenemedi: {DATA_ROOT / 'aircraft' / MODEL_NAME}")
        self.fdm.set_dt(PHYSICS_DT_S)

    # =========================================================================
    # BAŞLANGIÇ VE İLERLETME
    # =========================================================================

    def reset(self, lat_deg, lon_deg, alt_m, tas_mps, heading_deg):
        """Düz uçuşta başlat: konum (WGS84 jeodezik), irtifa MSL, gövde ekseni hızı, yön."""
        self.fdm["ic/lat-geod-deg"] = lat_deg
        self.fdm["ic/long-gc-deg"] = lon_deg
        self.fdm["ic/h-sl-ft"] = alt_m / METERS_PER_FOOT
        self.fdm["ic/u-fps"] = tas_mps / METERS_PER_FOOT
        self.fdm["ic/psi-true-rad"] = math.radians(heading_deg)
        self.fdm["propulsion/set-running"] = -1   # bütün motorlar çalışır (BVRGym FDMObject.reset)
        if not self.fdm.run_ic():
            raise RuntimeError("JSBSim başlangıç koşulu uygulanamadı.")
        self.fdm["gear/gear-pos-norm"] = 0         # iniş takımı yukarı (BVRGym set_retract_gear)

    def run(self):
        """Bir fizik adımı (PHYSICS_DT_S)."""
        self.fdm.run()

    # =========================================================================
    # KUMANDA — normalize: yüzeyler −1..1, gaz 0..1
    # =========================================================================

    def set_controls(self, aileron, elevator, rudder, throttle):
        self.fdm["fcs/aileron-cmd-norm"] = aileron
        self.fdm["fcs/elevator-cmd-norm"] = elevator
        self.fdm["fcs/rudder-cmd-norm"] = rudder
        self.fdm["fcs/throttle-cmd-norm"] = throttle

    # =========================================================================
    # DURUM — SI birimleri
    # =========================================================================

    @property
    def time_s(self):
        return self.fdm["simulation/sim-time-sec"]

    @property
    def lat_deg(self):
        return self.fdm["position/lat-geod-deg"]     # WGS84 jeodezik

    @property
    def lon_deg(self):
        return self.fdm["position/long-gc-deg"]

    @property
    def alt_m(self):
        return self.fdm["position/h-sl-meters"]

    @property
    def roll_deg(self):
        return self.fdm["attitude/phi-deg"]

    @property
    def pitch_deg(self):
        return self.fdm["attitude/theta-deg"]

    @property
    def yaw_deg(self):
        return self.fdm["attitude/psi-deg"]          # 0..360, gerçek kuzey

    @property
    def tas_mps(self):
        return self.fdm["velocities/vt-fps"] * METERS_PER_FOOT

    @property
    def mach(self):
        return self.fdm["velocities/mach"]

    @property
    def velocity_ned_mps(self):
        return [self.fdm[f"velocities/v-{axis}-fps"] * METERS_PER_FOOT for axis in ("north", "east", "down")]

    @property
    def load_factor(self):
        return self.fdm["forces/load-factor"]        # g, gövde normal yükü

    @property
    def qbar_pa(self):
        return self.fdm["aero/qbar-psf"] * 47.880259   # dinamik basınç; 1 lbf/ft² = 47.880259 Pa

    @property
    def body_rates_dps(self):
        """Gövde açısal hızları p (yatış), q (yunuslama), r (sapma), °/s."""
        return [self.fdm[f"velocities/{axis}-rad_sec"] * 180.0 / math.pi for axis in ("p", "q", "r")]

    @property
    def flight_path_deg(self):
        """Uçuş yolu açısı γ ve iz açısı χ (hız vektörünün yönü), derece."""
        north, east, down = self.velocity_ned_mps
        gamma = math.degrees(math.atan2(-down, math.hypot(north, east)))
        chi = math.degrees(math.atan2(east, north)) % 360.0
        return gamma, chi

    def kinematics(self):
        """C++'a giden ölçüm alanları (eski sim'in _kinematics'iyle aynı adlar)."""
        return dict(
            lat_deg=self.lat_deg, lon_deg=self.lon_deg, alt_msl_m=self.alt_m,
            yaw_deg=self.yaw_deg, pitch_deg=self.pitch_deg, roll_deg=self.roll_deg,
            tas_mps=self.tas_mps, mach=self.mach, velocity_ned_mps=self.velocity_ned_mps,
        )
