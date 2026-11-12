"""Ölçümleri ft/kt, C++ hesaplarını NM/kt ile kaydet. Biçim eski src/sim/recorder.py ile uyumlu
(görüntüleyici aynı okuyucuyla açar).

  <nesne>.csv       konum/yönelim + ham BVR alanları
  events.jsonl      fırlatma, isabet, kayıp, sağlık değişimi
  calculations.csv  C++ otopilot hesapları (geldiği gibi, ait olduğu ölçümün zamanıyla)
  manifest.json     senaryo, birimler, koordinat referansı, sütunlar

Yalnız birim dönüşümü yapılır; türetilmiş ölçüm hesaplanmaz.
"""
import csv
import json
import re
from datetime import datetime
from pathlib import Path

from simulator.physics.jsbsim_model import PHYSICS_DT_S

SCHEMA_VERSION = 4            # 3: BVRGym sim; 4: yeni simülatör (lat_gc_deg yok, simulation alanları değişti)
METERS_PER_FOOT = 0.3048
FPS_TO_KNOT = METERS_PER_FOOT * 3600.0 / 1852.0

TIME_COLUMN = ("time_s", "s")

# (sütun, JSBSim property, birim, çarpan). Enlem WGS84 jeodezik, irtifa h-sl-ft (eski sim'deki ölçümle aynı referans).
POSE_COLUMNS = (
    ("lat_deg", "position/lat-geod-deg", "deg", 1.0),
    ("lon_deg", "position/long-gc-deg", "deg", 1.0),
    ("alt_ft", "position/h-sl-ft", "ft", 1.0),
    ("roll_deg", "attitude/phi-deg", "deg", 1.0),
    ("pitch_deg", "attitude/theta-deg", "deg", 1.0),
    ("yaw_deg", "attitude/psi-deg", "deg", 1.0),
    ("mach", "velocities/mach", "-", 1.0),
)
EXTRA_COLUMNS = (
    ("tas_kt", "velocities/vtrue-kts", "kt", 1.0),
    ("vel_north_kt", "velocities/v-north-fps", "kt", FPS_TO_KNOT),
    ("vel_east_kt", "velocities/v-east-fps", "kt", FPS_TO_KNOT),
    ("vel_down_kt", "velocities/v-down-fps", "kt", FPS_TO_KNOT),
    ("load_factor_g", "forces/load-factor", "-", 1.0),
    ("fuel_lb", "propulsion/total-fuel-lbs", "lb", 1.0),
)
MPS_TO_KNOT = 3600.0 / 1852.0
# Füze: uçakla aynı adlar (görüntüleyici aynı okur), JSBSim yerine füze modelinden. (sütun, kaynak, birim)
MISSILE_COLUMNS = (
    ("lat_deg", "füze ECEF → WGS84 jeodezik", "deg"), ("lon_deg", "aynı", "deg"),
    ("alt_ft", "elipsoid yüksekliği", "ft"), ("roll_deg", "nokta kütle: 0", "deg"),
    ("pitch_deg", "uçuş yolu açısı", "deg"), ("yaw_deg", "iz açısı", "deg"), ("mach", "ISA ses hızına göre", "-"),
    ("tas_kt", "hız (hava durgun)", "kt"), ("vel_north_kt", "NED hız", "kt"), ("vel_east_kt", "NED hız", "kt"),
    ("vel_down_kt", "NED hız", "kt"), ("load_factor_g", "güdüm yükü (hıza dik)", "g"),
    ("missile_time_s", "füzenin uçuş süresi; atışta 0", "s"),
    ("guidance_mode", "none / datalink / inertial / seeker; kilitli: datalink, seeker. Pitbull anı events.jsonl'da bir kez", "-"),
)
COMMAND_COLUMNS = (
    ("cmd_heading_deg", "deg"),
    ("cmd_alt_ft", "ft"),
    ("cmd_throttle_norm", "-"),
    ("cmd_g_limit", "g"),
)

CALCULATIONS_FILE = "calculations.csv"
CALCULATION_KEYS = ("time_s", "id", "target_id")
AIRCRAFT_MODEL = "F-16"       # görüntüleyici dosya adındaki modelden 3B görünümü seçer


class Recorder:
    def __init__(self, directory, scenario=None, name=None):
        if name is None:
            name = datetime.now().strftime("%Y%m%d_%H%M%S_%f")
        self.directory = Path(directory) / name
        self.directory.mkdir(parents=True)            # aynı adlı kayıt varsa hata: üzerine yazılmaz
        self.scenario = scenario
        self.created_at = datetime.now().isoformat(timespec="seconds")
        self.files, self.writers, self.objects = {}, {}, {}
        self.snapshots = 0
        self.event_count = 0
        self.calculation_rows = 0
        self.previous_health = {}
        self.missile_state = {}                       # (uçak, slot) → önceki durum ve güdüm kipi (olay tespiti)
        self.finished_missiles = set()
        self.names = {key: agent["name"] for key, agent in ((scenario or {}).get("agents") or {}).items()}
        self.last_time = {}                           # uçak → son yazılan satırın time_s'i
        self.events = (self.directory / "events.jsonl").open("x", encoding="utf-8")
        self._write_manifest()

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_value, traceback):
        self.close()

    def close(self):
        for file in self.files.values():
            file.close()
        self.files.clear()
        self.events.close()
        self._write_manifest()

    # =========================================================================
    # SNAPSHOT
    # =========================================================================

    def record(self, simulation):
        """Bir snapshot: her uçağın ölçümü ve o an uygulanan komutu, havadaki füzeler, olaylar."""
        self.snapshots += 1
        for plane in simulation.aircraft.values():
            time_s = plane.model.time_s
            self.last_time[plane.key] = time_s
            self._detect_events(plane, time_s)
            self._record_aircraft(plane, time_s)
            for missile in plane.missiles:
                self._detect_missile_events(plane, missile, time_s)
                self._record_missile(plane, missile, time_s)

    def _record_missile(self, plane, missile, time_s):
        """Dosya ilk snapshot'ta (başlıkla) açılır; satırlar atıştan itibaren yazılır, uçuş bitince son satır bir kez."""
        key = (plane.key, missile.slot)
        filename = f"{missile.profile.name} ({plane.name}--{plane.key}--{missile.slot}) [{plane.team}].csv"
        columns = [TIME_COLUMN[0]] + [c for c, *_ in MISSILE_COLUMNS]
        self.objects.setdefault(filename, dict(
            file=filename, kind="missile", key=plane.key, carrier=plane.key, ammo_id=missile.slot,
            name=plane.name, team=plane.team, model=missile.profile.name, columns=columns))
        writer = self._writer(filename, columns)
        if missile.state == "ready" or key in self.finished_missiles:
            return
        state = missile.kinematics()
        north, east, down = state["velocity_ned_mps"]
        row = {TIME_COLUMN[0]: time_s, "lat_deg": state["lat_deg"], "lon_deg": state["lon_deg"],
               "alt_ft": state["alt_msl_m"] / METERS_PER_FOOT, "roll_deg": 0.0, "pitch_deg": state["pitch_deg"],
               "yaw_deg": state["yaw_deg"], "mach": state["mach"], "tas_kt": state["tas_mps"] * MPS_TO_KNOT,
               "vel_north_kt": north * MPS_TO_KNOT, "vel_east_kt": east * MPS_TO_KNOT,
               "vel_down_kt": down * MPS_TO_KNOT, "load_factor_g": missile.lateral_g,
               "missile_time_s": missile.time_s, "guidance_mode": missile.seeker.mode}
        writer.writerow(row)
        self.files[filename].flush()
        if missile.state == "ended":
            self.finished_missiles.add(key)

    def _detect_missile_events(self, plane, missile, time_s):
        key = (plane.key, missile.slot)
        was = self.missile_state.get(key, dict(state="ready", pitbull=False, locked=False))
        target = self._target_name(missile.target_key)
        if was["state"] == "ready" and missile.state != "ready":
            self._event(time_s, plane.key, "missile_launched", ammo=missile.slot, target=target)
        pitbull = missile.seeker.is_pitbull
        locked = missile.seeker.mode == "seeker"               # arayıcı hedefi görüyor
        if pitbull and not was["pitbull"]:                     # füze başına bir kez
            self._event(time_s, plane.key, "missile_pitbull", ammo=missile.slot, target=target)
        elif pitbull and locked and not was["locked"]:          # pitbull sonrası hedefi yeniden buldu
            self._event(time_s, plane.key, "missile_seeker_acquired", ammo=missile.slot, target=target)
        if was["locked"] and not locked and missile.state != "ended":   # hedefi kaybetti (isabette yanından geçiş sayılmaz)
            self._event(time_s, plane.key, "missile_seeker_lost", ammo=missile.slot, target=target)
        if missile.state == "ended" and was["state"] != "ended":
            kind = "missile_hit" if missile.outcome == "hit" else "missile_lost"
            self._event(time_s, plane.key, kind, ammo=missile.slot, target=target)
        self.missile_state[key] = dict(state=missile.state, pitbull=pitbull, locked=locked)

    def _target_name(self, key):
        return None if key is None else self.names.get(key, key)

    def _record_aircraft(self, plane, time_s):
        filename = f"{AIRCRAFT_MODEL} ({plane.name}--{plane.key}) [{plane.team}].csv"
        columns = [TIME_COLUMN[0]] + [c for c, *_ in POSE_COLUMNS + EXTRA_COLUMNS] + [c for c, _ in COMMAND_COLUMNS]
        self.objects.setdefault(filename, dict(
            file=filename, kind="aircraft", key=plane.key, name=plane.name, team=plane.team,
            model=AIRCRAFT_MODEL, ammo=plane.ammo, columns=columns))

        fdm = plane.model.fdm
        row = {TIME_COLUMN[0]: time_s}
        for column, name, _, scale in POSE_COLUMNS + EXTRA_COLUMNS:
            row[column] = fdm[name] * scale
        command = plane.command
        row.update(cmd_heading_deg=command.heading_deg, cmd_alt_ft=command.altitude_m / METERS_PER_FOOT,
                   cmd_throttle_norm=command.throttle, cmd_g_limit=command.g_limit)   # g_limit yoksa hücre boş
        self._writer(filename, columns).writerow(row)
        self.files[filename].flush()

    # =========================================================================
    # OLAYLAR
    # =========================================================================

    def _detect_events(self, plane, time_s):
        previous = self.previous_health.get(plane.key)
        if previous is not None and previous != plane.health:
            self._event(time_s, plane.key, "health_changed", **{"from": previous, "to": plane.health})
        self.previous_health[plane.key] = plane.health

    def _event(self, time_s, agent_key, kind, **extra):
        self.event_count += 1
        record = dict(seq=self.event_count, sim_time_s=time_s, agent=agent_key, type=kind)
        record.update(extra)
        self.events.write(json.dumps(record, ensure_ascii=False) + "\n")
        self.events.flush()

    # =========================================================================
    # C++ HESAPLARI — kullanılan ölçümün zamanıyla
    # =========================================================================

    def record_calculations(self, calculations):
        """C++'ın son state üzerinden yaptığı hesapları yaz; satır zamanı o uçağın son yazılan time_s'i."""
        if calculations is None:
            return
        if not isinstance(calculations, dict) or not isinstance(calculations.get("aircraft"), list):
            raise ValueError("calculations.aircraft liste olmalı.")
        for item in calculations["aircraft"]:
            if not isinstance(item, dict) or item.get("id") not in self.last_time:
                raise ValueError(f"Hesapta bilinmeyen uçak id: {item.get('id') if isinstance(item, dict) else item}")
            if "time_s" in item:
                raise ValueError("Hesapta time_s olmamalı; zamanı kayıt koyar.")
            row = {"time_s": self.last_time[item["id"]]}
            row.update({key: int(value) if isinstance(value, bool) else value for key, value in item.items()})
            columns = [key for key in CALCULATION_KEYS if key in row]
            columns += [key for key in item if key not in CALCULATION_KEYS]
            writer = self._writer(CALCULATIONS_FILE, columns)
            if set(row) != set(writer.fieldnames):
                raise ValueError(f"{item['id']}: hesap alanları ilk mesajdakinden farklı.")
            writer.writerow(row)
            self.calculation_rows += 1
        if CALCULATIONS_FILE in self.files:
            self.files[CALCULATIONS_FILE].flush()

    # =========================================================================
    # DOSYA VE MANİFEST
    # =========================================================================

    def _writer(self, filename, columns):
        if filename not in self.files:
            safe_name = re.sub(r'[<>:"/\\|?*\x00-\x1f]', "_", filename)
            file = (self.directory / safe_name).open("x", newline="", encoding="utf-8")
            self.files[filename] = file
            self.writers[filename] = csv.DictWriter(file, fieldnames=list(columns))
            self.writers[filename].writeheader()
            file.flush()                              # atılmamış füzenin dosyası da başlıkla diskte olsun
        return self.writers[filename]

    def _write_manifest(self):
        columns = {TIME_COLUMN[0]: dict(source="uçağın JSBSim saati", unit=TIME_COLUMN[1])}
        for column, name, unit, scale in POSE_COLUMNS + EXTRA_COLUMNS:
            columns[column] = dict(source=name, unit=unit, scale=scale)
        for column, unit in COMMAND_COLUMNS:
            columns[column] = dict(source="dış uçuş komutu", unit=unit)
        for column, source, unit in MISSILE_COLUMNS:
            columns.setdefault(column, dict(unit=unit))["missile_source"] = source
        steps = (self.scenario or {}).get("world", {}).get("physics_steps_per_tick")
        manifest = dict(
            schema_version=SCHEMA_VERSION,
            kind="simulator-bvr-recording",
            created_at=self.created_at,
            snapshots=self.snapshots,
            events=self.event_count,
            coordinates=dict(
                latitude_reference="WGS84 geodetic",
                longitude_reference="WGS84",
                altitude_reference="WGS84 ellipsoid height",
                note="Başlangıç enlemi ic/lat-geod-deg'e yazılır; lat_deg ile senaryo enlemi aynı referanstadır.",
            ),
            conversions="irtifa ft; hız kt; C++ mesafeleri NM; NED hızları ft/s → kt; komut irtifası m → ft",
            columns=columns,
            simulation=dict(engine="JSBSim", physics_dt_s=PHYSICS_DT_S,
                            aircraft_control_time_step=steps),   # görüntüleyicinin oturum panelindeki alan adı
            scenario=self.scenario,
            objects=list(self.objects.values()),
            files=dict(events="events.jsonl", calculations=CALCULATIONS_FILE),
            calculations=dict(rows=self.calculation_rows,
                              source="C++ otopilot (command mesajının calculations alanı)",
                              units=dict(distance="NM", speed="kt", angle="deg", time="s")),
        )
        (self.directory / "manifest.json").write_text(json.dumps(manifest, ensure_ascii=False, indent=2),
                                                      encoding="utf-8")
