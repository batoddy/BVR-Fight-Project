"""Menzil taraması: sim füzesi (weapons/missile.py) için erişilen en büyük atış mesafesi ve hız-zaman tablosu.

Workspace içinden: .venv/Scripts/python.exe -X utf8 src/simulator/tools/menzil_tarama.py [--deneme] [--kaba]
Çıktı: src/otopilot/olcumler/faz4_olcum/menzil_olcumler.csv, hiz_zaman.csv
Erişim: füze uçuştayken hedef füzenin önünde değilse (dot(hedef − füze, füze hızı) ≤ 0) ya da isabet olduysa.
"""
import csv
import math
import sys
import time
from pathlib import Path

import numpy as np

SRC = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(SRC))

from simulator import data
from simulator.physics import earth
from simulator.weapons.missile import Missile

OUT = SRC / "otopilot" / "olcumler" / "faz4_olcum"
DT = 1.0 / 120.0
KT = 1852.0 / 3600.0            # m/s cinsinden 1 kt
NM = 1852.0
EARTH_RADIUS_M = 6371000.0
PRECISION_NM = 0.15
MIN_NM, MAX_NM = 1.0, 150.0
ASPECT_HEADING = {"hot": 0.0, "beam": 90.0, "cold": 180.0}
ALTITUDES = (4000, 6000, 8000, 10000)
TARGET_SPEEDS = (220, 300, 380)
SHOOTER_SPEEDS = (220, 300, 380)
FIELDS = ["altitude_m", "aspect", "target_speed_mps", "shooter_speed_mps", "range_nm", "flight_time_s",
          "missile_speed_kt", "hit"]


class _Radar:
    def sees(self, own, target):
        return True             # kusursuz veri bağı


class _Model:
    def __init__(self, lat, lon, alt, speed, heading_deg):
        self.lat_deg, self.lon_deg, self.alt_m = lat, lon, alt
        h = math.radians(heading_deg)
        self.velocity_ned_mps = [speed * math.cos(h), speed * math.sin(h), 0.0]
        self.yaw_deg, self.pitch_deg, self.roll_deg = heading_deg, 0.0, 0.0


class PointAircraft:
    """Sabit hız, irtifa ve başlıkla düz uçan nokta uçak."""

    def __init__(self, lat, lon, alt, speed, heading_deg):
        self.model = _Model(lat, lon, alt, speed, heading_deg)
        self.radar = _Radar()
        self.health = 1
        self.forward_ecef = earth.ned_to_ecef([math.cos(math.radians(heading_deg)),
                                               math.sin(math.radians(heading_deg)), 0.0], lat, lon)

    def position_ecef(self):
        return earth.geodetic_to_ecef(self.model.lat_deg, self.model.lon_deg, self.model.alt_m)

    def velocity_ecef(self):
        return earth.ned_to_ecef(self.model.velocity_ned_mps, self.model.lat_deg, self.model.lon_deg)

    def advance(self, dt):
        lat, lon, _ = earth.ecef_to_geodetic(self.position_ecef() + self.velocity_ecef() * dt)
        self.model.lat_deg, self.model.lon_deg = lat, lon       # irtifa sabit


def _setup(profile, altitude_m, range_nm, shooter_mps, target_mps, aspect):
    lat0 = 40.0
    shooter = PointAircraft(lat0, 30.0, altitude_m, shooter_mps, 180.0)
    d_lat = math.degrees(range_nm * NM / (EARTH_RADIUS_M + altitude_m))
    target = PointAircraft(lat0 - d_lat, 30.0, altitude_m, target_mps, ASPECT_HEADING[aspect])
    missile = Missile(profile, 0)
    missile.launch(shooter.model, "target")
    return shooter, target, missile


def fly(profile, altitude_m, range_nm, shooter_mps, target_mps, aspect):
    """Atışı uçur. Dönüş: (erişti, uçuş süresi s, erişimdeki füze hızı kt, isabet mi)."""
    shooter, target, missile = _setup(profile, altitude_m, range_nm, shooter_mps, target_mps, aspect)
    while True:
        missile.step(DT, shooter, target)
        shooter.advance(DT)
        target.advance(DT)
        speed_kt = float(np.linalg.norm(missile.velocity)) / KT
        if missile.state == "ended":
            hit = missile.outcome == "hit"
            return hit, missile.time_s, speed_kt, hit
        if np.dot(target.position_ecef() - missile.position, missile.velocity) <= 0.0:
            return True, missile.time_s, speed_kt, False


def boundary(profile, altitude_m, shooter_mps, target_mps, aspect):
    """İkiye bölmeyle erişilen en büyük mesafe (NM) ve o atışın bilgileri; 1 NM'de erişilemezse None."""
    reach = fly(profile, altitude_m, MIN_NM, shooter_mps, target_mps, aspect)
    if not reach[0]:
        return None
    low, best = MIN_NM, reach
    high = MAX_NM
    top = fly(profile, altitude_m, MAX_NM, shooter_mps, target_mps, aspect)
    if top[0]:
        return MAX_NM, top
    while high - low > PRECISION_NM:
        mid = 0.5 * (low + high)
        result = fly(profile, altitude_m, mid, shooter_mps, target_mps, aspect)
        if result[0]:
            low, best = mid, result
        else:
            high = mid
    return low, best


def scenarios():
    for alt in ALTITUDES:
        for vt in TARGET_SPEEDS:
            for aspect in ("hot", "beam", "cold"):
                yield alt, aspect, vt, 300
    for vs in SHOOTER_SPEEDS:
        for aspect in ("hot", "cold"):
            yield 8000, aspect, 300, vs


def run_boundaries(profile):
    path = OUT / "menzil_olcumler.csv"
    done = set()
    if path.exists():
        with path.open(newline="", encoding="utf-8") as file:
            done = {(int(r["altitude_m"]), r["aspect"], int(r["target_speed_mps"]), int(r["shooter_speed_mps"]))
                    for r in csv.DictReader(file)}
    new_file = not path.exists()
    with path.open("a", newline="", encoding="utf-8") as file:
        writer = csv.writer(file)
        if new_file:
            writer.writerow(FIELDS)
        for key in dict.fromkeys(scenarios()):
            if key in done:
                continue
            start = time.time()
            result = boundary(profile, key[0], key[3], key[2], key[1])
            if result is None:
                row = [*key[:2], key[2], key[3], "", "", "", ""]
            else:
                range_nm, (_, t, v_kt, hit) = result
                row = [key[0], key[1], key[2], key[3], f"{range_nm:.3f}", f"{t:.2f}", f"{v_kt:.1f}", int(hit)]
            writer.writerow(row)
            file.flush()
            print(row, f"{time.time() - start:.0f} s", flush=True)


def coarse_scan(profile):
    """Tek yönlülük: 5 NM aralıkla tara, ilk erişilemeyen mesafeden sonra tekrar erişim var mı."""
    for key in ((8000, "hot", 300, 300), (4000, "hot", 380, 300), (10000, "cold", 380, 300), (6000, "beam", 300, 300),
                (10000, "hot", 220, 300), (4000, "cold", 220, 300)):
        flags = [fly(profile, key[0], r, key[3], key[2], key[1])[0] for r in np.arange(1.0, 150.1, 5.0)]
        first_fail = flags.index(False) if False in flags else None
        again = first_fail is not None and any(flags[first_fail:])
        print(key, "ilk erişilemeyen:", None if first_fail is None else 1.0 + 5.0 * first_fail,
              "| sonra tekrar erişim:", again, flush=True)


def run_speed_time(profile):
    path = OUT / "hiz_zaman.csv"
    with path.open("w", newline="", encoding="utf-8") as file:
        writer = csv.writer(file)
        writer.writerow(["altitude_m", "time_s", "missile_speed_kt"])
        for alt in ALTITUDES:
            shooter, target, missile = _setup(profile, alt, MAX_NM, 300, 0.0, "hot")
            step = 0
            while missile.state == "in_flight":
                if step % 60 == 0:
                    writer.writerow([alt, f"{step * DT:.1f}", f"{np.linalg.norm(missile.velocity) / KT:.1f}"])
                missile.step(DT, shooter, target)
                shooter.advance(DT)
                target.advance(DT)
                step += 1
            print(alt, "m: füze bitti", f"{missile.time_s:.1f} s", missile.outcome, flush=True)


if __name__ == "__main__":
    OUT.mkdir(parents=True, exist_ok=True)
    prof = data.load_missile("aim120c5")
    if "--deneme" in sys.argv:
        start = time.time()
        print(boundary(prof, 8000, 300, 300, "hot"), f"{time.time() - start:.0f} s")
    elif "--kaba" in sys.argv:
        coarse_scan(prof)
    else:
        run_speed_time(prof)
        run_boundaries(prof)
