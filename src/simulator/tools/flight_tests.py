"""Uçuş kontrolü testleri: senaryoları C++'sız uçurur, ölçütleri tablo halinde yazdırır.

Workspace içinden:
  python src/simulator/tools/flight_tests.py                 yeni sim (AMLController)
  python src/simulator/tools/flight_tests.py --old           eski sim (src/sim, BVRGym + eski flight_control) kıyası
  python src/simulator/tools/flight_tests.py --only out crank --record   recordings/ucus_testi_<ad> kaydı (yalnız yeni sim)

Ölçütler plan §3.3. Yön, hız vektörünün iz açısıdır (χ). Ölçüm koda alan eklemeden, JSBSim property'lerinden okunur.
"""
import argparse
import math
import sys
from dataclasses import dataclass
from datetime import datetime
from pathlib import Path

SRC = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(SRC))

from simulator.physics.jsbsim_model import METERS_PER_FOOT

WORKSPACE = SRC.parent
HEADING_BAND_DEG = 2.0       # plan §3.3: dönüş sonunda yön ±2°
ALTITUDE_BAND_M = 50.0       # plan §3.3: irtifa ±50 m
P_DEADBAND_DPS = 0.5         # °/s, salınım sayımında yok sayılan yatış hızı (ölçüm gürültüsü)
SPEED_OF_SOUND_10KM = 299.5  # m/s, ISA 10 km (223.15 K); başlangıç hızı M0.9
SPEED_OF_SOUND_5KM = 320.5   # m/s, ISA 5 km (255.65 K)


@dataclass
class Case:
    name: str
    altitude_m: float
    speed_mps: float
    throttle: float
    heading_change_deg: float     # +: sağa
    altitude_change_m: float
    g_limit: float | None
    duration_s: float


CASES = [
    Case("out", 10000, 0.9 * SPEED_OF_SOUND_10KM, 1.0, 180.0, 0, 6.5, 60),
    Case("crank", 10000, 0.9 * SPEED_OF_SOUND_10KM, 0.45, 50.0, 0, 3.0, 40),
    Case("kucuk", 10000, 0.9 * SPEED_OF_SOUND_10KM, 0.45, 5.0, 0, 3.0, 30),
    Case("irtifa_yukari", 10000, 0.9 * SPEED_OF_SOUND_10KM, 0.45, 0.0, 2000, None, 90),
    Case("irtifa_asagi", 10000, 0.9 * SPEED_OF_SOUND_10KM, 0.45, 0.0, -2000, None, 90),
    Case("duz", 10000, 0.9 * SPEED_OF_SOUND_10KM, 0.45, 0.0, 0, None, 60),
    Case("out_5km", 5000, 0.9 * SPEED_OF_SOUND_5KM, 1.0, 180.0, 0, 6.5, 60),
    Case("kucuk_5km", 5000, 0.9 * SPEED_OF_SOUND_5KM, 0.45, 5.0, 0, 3.0, 30),   # titreşim yüksek q̄'da daha olası
]


# =============================================================================
# İKİ SİMÜLATÖR İÇİN AYNI ARAYÜZ: fdm, time_s, step(heading, altitude, throttle, g_limit)
# =============================================================================

def one_aircraft_scenario(case, world):
    agent = dict(name="TEST", type="F16", team="Blue", ammo=0, throttle=case.throttle,
                 spawn=dict(lat=59.0, long=18.0, alt=case.altitude_m, vel=case.speed_mps, heading=0.0))
    return dict(world=dict(duration_s=case.duration_s + 1.0, sleep_s=0.0, **world), agents=dict(blue=agent))


def recording_name(case):
    """recordings/ucus_testi_<ad>; varsa üzerine yazılmaz, tarih-saat eklenir."""
    name = f"ucus_testi_{case.name}"
    if (WORKSPACE / "recordings" / name).exists():
        name += datetime.now().strftime("_%Y%m%d_%H%M%S")
    return name


class NewSim:
    def __init__(self, case, record):
        from simulator.flight_control.aml import AMLController
        from simulator.recorder import Recorder
        from simulator.simulation import Simulation
        scenario = one_aircraft_scenario(case, dict(physics_steps_per_tick=10))
        self.simulation = Simulation(scenario, controller_factory=AMLController)
        self.fdm = self.simulation.aircraft["blue"].model.fdm
        self.recorder = Recorder(WORKSPACE / "recordings", scenario, recording_name(case)) if record else None
        if self.recorder:
            self.recorder.record(self.simulation)

    def step(self, heading_deg, altitude_m, throttle, g_limit):
        command = dict(id="blue", heading_deg=heading_deg, altitude_m=altitude_m, throttle=throttle)
        if g_limit is not None:
            command["g_limit"] = g_limit
        self.simulation.step([command])
        if self.recorder:
            self.recorder.record(self.simulation)

    def close(self):
        if self.recorder:
            self.recorder.close()
            print("Kayıt:", self.recorder.directory)


class OldSim(NewSim):
    """Eski src/sim (BVRGym + eski flight_control). Eski dosyalar değişmez, yalnız içe aktarılır."""

    def __init__(self, case, record):
        from sim.simulation import Simulation
        scenario = one_aircraft_scenario(case, dict(sim_time_step=1))
        self.simulation = Simulation(scenario, WORKSPACE / "BVRGym")
        self.fdm = self.simulation.agents["blue"].simObj.fdm
        self.recorder = None


# =============================================================================
# KOŞU VE ÖLÇÜM
# =============================================================================

def wrap180(angle):
    return (angle + 180.0) % 360.0 - 180.0


def sample(fdm):
    north, east = fdm["velocities/v-north-fps"], fdm["velocities/v-east-fps"]
    return dict(t=fdm["simulation/sim-time-sec"], alt=fdm["position/h-sl-ft"] * METERS_PER_FOOT,
                track=math.degrees(math.atan2(east, north)) % 360.0, mach=fdm["velocities/mach"],
                g=fdm["forces/load-factor"], p=math.degrees(fdm["velocities/p-rad_sec"]), roll=fdm["attitude/phi-deg"])


def fly(case, simulator):
    """Bir senaryoyu uçur; örnek listesi döndür. Komut ilk adımdan itibaren sabittir."""
    start = sample(simulator.fdm)
    heading = (start["track"] + case.heading_change_deg) % 360.0
    altitude = start["alt"] + case.altitude_change_m
    samples = [start]
    while samples[-1]["t"] - start["t"] < case.duration_s - 1e-6:
        simulator.step(heading, altitude, case.throttle, case.g_limit)
        samples.append(sample(simulator.fdm))
    simulator.close()
    return samples, heading, altitude


def settle_time(samples, error, band):
    """Hatanın band içine girip bir daha çıkmadığı ilk an; hiç oturmazsa None."""
    for index in range(len(samples) - 1, -1, -1):
        if abs(error(samples[index])) > band:
            return None if index == len(samples) - 1 else samples[index + 1]["t"] - samples[0]["t"]
    return 0.0


def sign_changes(values, deadband):
    """Ölü bandın dışındaki işaret değişimi sayısı: salınım göstergesi."""
    signs = [math.copysign(1.0, v) for v in values if abs(v) > deadband]
    return sum(1 for a, b in zip(signs, signs[1:]) if a != b)


def measure(case, samples, heading, altitude):
    start = samples[0]
    heading_error = lambda s: wrap180(s["track"] - heading)
    altitude_error = lambda s: s["alt"] - altitude
    # aşma: hedefe yaklaşılan taraftan karşı tarafa geçilen en büyük açı (180°'de dönüş yönü uçuştan okunur)
    late = [s for s in samples if abs(heading_error(s)) < 90.0] if case.heading_change_deg else []
    side = math.copysign(1.0, heading_error(late[0])) if late else 0.0
    overshoot = max([-side * heading_error(s) for s in late] + [0.0])
    t_heading = settle_time(samples, heading_error, HEADING_BAND_DEG)
    t_altitude = settle_time(samples, altitude_error, ALTITUDE_BAND_M)
    after = [s for s in samples if t_heading is not None and s["t"] - start["t"] >= t_heading]
    after_alt = [s for s in samples if t_altitude is not None and s["t"] - start["t"] >= t_altitude]
    if case.altitude_change_m:
        alt_overshoot = max(math.copysign(1.0, case.altitude_change_m) * altitude_error(s) for s in samples)
    else:
        alt_overshoot = None
    return dict(
        t_hdg=t_heading,
        asma_hdg=overshoot,
        hdg_max_sonra=max((abs(heading_error(s)) for s in after), default=None),
        hdg_son=heading_error(samples[-1]),
        t_alt=t_altitude,
        asma_alt=alt_overshoot,
        alt_min=min(s["alt"] for s in samples) - start["alt"],
        alt_max=max(s["alt"] for s in samples) - start["alt"],
        alt_son=altitude_error(samples[-1]),
        mach0=start["mach"], mach_son=samples[-1]["mach"],
        g_max=max(s["g"] for s in samples),
        p_rms_sonra=math.sqrt(sum(s["p"] ** 2 for s in after) / len(after)) if after else None,
        p_isaret=sign_changes([s["p"] for s in after], P_DEADBAND_DPS) if after else None,
    )


# =============================================================================
# TABLO
# =============================================================================

COLUMNS = (   # (başlık, anahtar, biçim)
    ("senaryo", "name", "{}"), ("t±2°", "t_hdg", "{:.1f}"), ("aşma°", "asma_hdg", "{:.2f}"),
    ("|ψe|maks sonra", "hdg_max_sonra", "{:.2f}"), ("ψe son", "hdg_son", "{:.2f}"),
    ("t±50m", "t_alt", "{:.1f}"), ("aşma m", "asma_alt", "{:.0f}"), ("Δh min", "alt_min", "{:.0f}"),
    ("Δh maks", "alt_max", "{:.0f}"), ("he son", "alt_son", "{:.0f}"), ("M0", "mach0", "{:.2f}"),
    ("M son", "mach_son", "{:.2f}"), ("g maks", "g_max", "{:.2f}"), ("p RMS °/s", "p_rms_sonra", "{:.2f}"),
    ("p işaret", "p_isaret", "{}"),
)


def print_table(title, rows):
    print(f"\n{title}")
    cells = [[c[0] for c in COLUMNS]]
    for row in rows:
        cells.append(["—" if row.get(key) is None else form.format(row[key]) for _, key, form in COLUMNS])
    widths = [max(len(r[i]) for r in cells) for i in range(len(COLUMNS))]
    for index, line in enumerate(cells):
        print(" | ".join(text.rjust(width) for text, width in zip(line, widths)))
        if index == 0:
            print("-+-".join("-" * width for width in widths))


def main():
    parser = argparse.ArgumentParser(description="Uçuş kontrolü testleri (plan §3.3).")
    parser.add_argument("--old", action="store_true", help="Eski sim (src/sim, BVRGym) ile koş.")
    parser.add_argument("--only", nargs="+", help="Yalnız bu senaryolar: " + ", ".join(c.name for c in CASES))
    parser.add_argument("--record", action="store_true", help="recordings/ucus_testi_<ad> kaydı (yeni sim).")
    arguments = parser.parse_args()
    if arguments.old and arguments.record:
        parser.error("--record yalnız yeni sim içindir.")
    simulator = OldSim if arguments.old else NewSim
    rows = []
    for case in CASES:
        if arguments.only and case.name not in arguments.only:
            continue
        samples, heading, altitude = fly(case, simulator(case, arguments.record))
        rows.append(dict(name=case.name, **measure(case, samples, heading, altitude)))
    print_table("ESKİ SİM (BVRGym + eski flight_control)" if arguments.old else "YENİ SİM (AMLController)", rows)


if __name__ == "__main__":
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8")
    main()
