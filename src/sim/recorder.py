"""BVR ölçümlerini ft/kt, C++ hesaplarını NM/kt birimleriyle kaydet.

Dört dosya üretilir:
  <nesne>.csv       Konum/yönelim + Tacview karşılığı olmayan ham BVR alanları.
  events.jsonl      Hedef ataması, fırlatma, isabet, kayıp, sağlık değişimi.
  calculations.csv  C++ otopilotun hesapları (ATA, bearing, range, WEZ ...). Burada
                    hesaplanmaz; C++'tan geldiği gibi, ait olduğu ölçümün zamanıyla yazılır.
  manifest.json     Senaryo, birimler, koordinat referansı, sütun kaynakları, sürümler.

Yalnız birim dönüşümü yapılır; taktik veya türetilmiş ölçüm hesaplanmaz.
Alan modelde yoksa sütun açılmaz; sıfır değer uydurulmaz.
"""
import csv
import json
import re
from datetime import datetime
from pathlib import Path


# =============================================================================
# KAYIT ŞEMASI — sütun adı, JSBSim kaynağı ve birim
# =============================================================================

SCHEMA_VERSION = 3
METERS_PER_FOOT = 0.3048
FPS_TO_KNOT = METERS_PER_FOOT * 3600.0 / 1852.0

# Zaman sütunu modelden okunmaz; record() çağrısına verilen taşıyıcı saatidir.
TIME_COLUMN = ("time_s", "s")

# Her nesnede bulunan konum ve yönelim. Enlem WGS84 jeodeziktir: JSBSim'in
# position/lat-geod-deg'i, position/h-sl-ft ise elipsoid yüksekliğidir
# (geod-alt ile farkı ölçüldü: 1,3 cm'den küçük). Boylamda jeosantrik ile
# jeodezik aynı değerdir, dönüşüm gerekmez.
# Son eleman kaynak değere uygulanacak birim çarpanıdır; 1.0 doğrudan okumadır.
POSE_COLUMNS = (
    ("lat_deg", "position/lat-geod-deg", "deg", 1.0),
    ("lon_deg", "position/long-gc-deg", "deg", 1.0),
    ("alt_ft", "position/h-sl-ft", "ft", 1.0),
    ("roll_deg", "attitude/phi-deg", "deg", 1.0),
    ("pitch_deg", "attitude/theta-deg", "deg", 1.0),
    ("yaw_deg", "attitude/psi-deg", "deg", 1.0),
    ("mach", "velocities/mach", "-", 1.0),
)

# BVR kararı ve değerlendirmesi için gereken ek ham alanlar. Modelde okunabilir
# olmayan atlanır. lat_gc_deg BVRGym'in iç referansıdır: spawn ic/lat-gc-deg'e
# yazıldığı için scenario.yaml'daki enlemle eşleşen sütun budur, lat_deg değil.
# Kuvvet/moment toplamları, atalet, atmosfer, rüzgâr, yer teması, kontrol yüzeyi
# konumları, gövde hızları, açısal hızlar ve alpha/beta bilinçli olarak dışarıda:
# ne sahnede çizilir ne BVR kararına girer. Gerekirse buraya eklenir.
EXTRA_COLUMNS = (
    ("lat_gc_deg", "position/lat-gc-deg", "deg", 1.0),
    ("tas_kt", "velocities/vtrue-kts", "kt", 1.0),
    ("vel_north_kt", "velocities/v-north-fps", "kt", FPS_TO_KNOT),
    ("vel_east_kt", "velocities/v-east-fps", "kt", FPS_TO_KNOT),
    ("vel_down_kt", "velocities/v-down-fps", "kt", FPS_TO_KNOT),
    ("load_factor_g", "forces/load-factor", "-", 1.0),
    ("fuel_lb", "propulsion/total-fuel-lbs", "lb", 1.0),
)

# Füzenin kendi saati. time_s taşıyıcının saatidir; bu füzenin kendisidir.
MISSILE_COLUMNS = (("missile_time_s", "simulation/sim-time-sec", "s", 1.0),)

# Dışarıdan verilen uçuş hedefi. Ölçüm değil, bizim komutumuz; olgu olarak
# yazılır. JSBSim'in fcs/throttle-cmd-norm'u ayrı sütun değildir: uçakta
# set_throttle() doğrudan cmd_throttle_norm'u yazar, füzede autopilot sabit
# 0,7 yazar — iki durumda da bilgi taşımaz.
COMMAND_COLUMNS = (
    ("cmd_heading_deg", "deg"),
    ("cmd_alt_ft", "ft"),
    ("cmd_throttle_norm", "-"),
)

# C++ hesap dosyası. Sütunları C++ belirler (ilk mesajdaki anahtarlar); başta bunlar.
CALCULATIONS_FILE = "calculations.csv"
CALCULATION_KEYS = ("time_s", "id", "target_id")

MISSILE_MODEL = "AIM-120 AMRAAM"
AIRCRAFT_MODEL = "F-16"


class Recorder:
    # =========================================================================
    # KAYIT KLASÖRÜ VE DOSYALARIN YAŞAM DÖNGÜSÜ
    # =========================================================================

    def __init__(self, directory, scenario=None):
        self.directory = Path(directory) / datetime.now().strftime("%Y%m%d_%H%M%S_%f")
        self.directory.mkdir(parents=True)
        self.scenario = scenario
        self.created_at = datetime.now().isoformat(timespec="seconds")
        self.files = {}
        self.writers = {}
        self.properties = {}
        self.objects = {}
        self.snapshots = 0
        self.finished_missiles = set()
        self.state = {}
        self.event_count = 0
        self.last_time = {}          # ajan -> son yazılan uçak satırının time_s'i
        self.calculation_rows = 0
        self.events = (self.directory / "events.jsonl").open("x", encoding="utf-8")
        self.simulation = None
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
    # CSV YARDIMCILARI — dosyayı açma, okunabilir alanları bulma, satır hazırlama
    # =========================================================================

    def _writer(self, filename, columns):
        if filename not in self.files:
            safe_name = re.sub(r'[<>:"/\\|?*\x00-\x1f]', "_", filename)
            file = (self.directory / safe_name).open("x", newline="", encoding="utf-8")
            self.files[filename] = file
            self.writers[filename] = csv.DictWriter(file, fieldnames=list(columns))
            self.writers[filename].writeheader()
            file.flush()
        return self.writers[filename]

    def _readable(self, prefix, model, extra):
        """Modelde okunabilen (sütun, property, çarpan) üçlülerini bir kez belirle."""
        if prefix not in self.properties:
            readable = set()
            for entry in model.fdm.get_property_catalog():
                name, permission = entry.rsplit(" (", 1)
                if "R" in permission:
                    readable.add(name)
            self.properties[prefix] = [(column, name, scale) for column, name, _, scale in extra
                                       if name in readable]
        return self.properties[prefix]

    def _columns(self, prefix, model, extra, command=False):
        columns = [TIME_COLUMN[0]]
        columns += [column for column, _, _, _ in POSE_COLUMNS]
        columns += [column for column, _, _ in self._readable(prefix, model, extra)]
        if command:
            columns += [column for column, _ in COMMAND_COLUMNS]
        return columns

    def _row(self, prefix, model, extra, time_s, command=None):
        values = {TIME_COLUMN[0]: time_s}
        for column, name, _, scale in POSE_COLUMNS:
            values[column] = model.fdm[name] * scale
        for column, name, scale in self._readable(prefix, model, extra):
            values[column] = model.fdm[name] * scale
        if command is not None:
            values.update(command)
        return values

    # =========================================================================
    # OLAYLAR — BVRGym bayraklarındaki değişimleri kaydetme
    # =========================================================================

    def _event(self, time_s, agent_key, kind, **extra):
        self.event_count += 1
        record = dict(seq=self.event_count, sim_time_s=time_s, agent=agent_key, type=kind)
        record.update(extra)
        self.events.write(json.dumps(record, ensure_ascii=False) + "\n")
        self.events.flush()

    @staticmethod
    def _target_name(holder):
        return None if holder.target is None else holder.target.conf.agent_name

    def _detect_events(self, key, agent, time_s):
        """Durum değişimlerini olaya çevir. Zaman, değişimin görüldüğü snapshot'ın
        zamanıdır; çözünürlük record() aralığı kadardır."""
        previous = self.state.get(key, {})
        current = {"target": self._target_name(agent), "health": agent.healthPoints}
        if previous.get("target") != current["target"]:
            self._event(time_s, key, "target_assigned" if current["target"] else "target_cleared",
                        target=current["target"])
        if previous and previous.get("health") != current["health"]:
            self._event(time_s, key, "health_changed",
                        **{"from": previous["health"], "to": current["health"]})
        for ammo_id, missile in agent.ammo.items():
            was = previous.get(ammo_id, {})
            now = {"active": missile.active, "hit": missile.target_hit,
                   "lost": missile.target_lost, "alive": missile.alive}
            if now["active"] and not was.get("active"):
                self._event(time_s, key, "missile_launched", ammo=ammo_id,
                            target=self._target_name(missile))
            if now["hit"] and not was.get("hit"):
                self._event(time_s, key, "missile_hit", ammo=ammo_id,
                            target=self._target_name(missile))
            if now["lost"] and not was.get("lost"):
                self._event(time_s, key, "missile_lost", ammo=ammo_id,
                            target=self._target_name(missile))
            current[ammo_id] = now
        self.state[key] = current

    # =========================================================================
    # BİR SNAPSHOT KAYDETME — olaylar → uçak ölçümü → füze ölçümleri
    # =========================================================================

    def record(self, agents, commands=None):
        """Her çağrı bir snapshot yazar; bütün fizik alt adımları değildir."""
        self.snapshots += 1
        for key, agent in agents.items():
            time_s = agent.simObj.fdm["simulation/sim-time-sec"]
            self.last_time[key] = time_s
            if self.simulation is None:
                self._read_simulation(agent)

            self._detect_events(key, agent, time_s)
            self._record_aircraft(key, agent, time_s, commands)
            self._record_missiles(key, agent, time_s)

    def _record_aircraft(self, key, agent, time_s, commands):
        """Bir uçağın ham ölçümünü ve varsa o anda uygulanan uçuş hedefini yaz."""
        name, team = agent.conf.agent_name, agent.conf.team
        command = None
        if commands is not None and key in commands:
            heading_deg, altitude_m, throttle = commands[key]
            # Python uçuş komutu metre; CSV irtifa komutu feet olarak saklanır.
            command = dict(cmd_heading_deg=heading_deg,
                           cmd_alt_ft=altitude_m / METERS_PER_FOOT,
                           cmd_throttle_norm=throttle)

        prefix = f"{key}__aircraft"
        filename = f"{AIRCRAFT_MODEL} ({name}--{key}) [{team}].csv"
        columns = self._columns(prefix, agent.simObj, EXTRA_COLUMNS, command=True)
        self.objects.setdefault(filename, dict(
            file=filename, kind="aircraft", key=key, name=name, team=team,
            model=AIRCRAFT_MODEL, ammo=len(agent.ammo), columns=columns))

        row = self._row(prefix, agent.simObj, EXTRA_COLUMNS, time_s, command)
        self._writer(filename, columns).writerow(row)
        self.files[filename].flush()

    def _record_missiles(self, key, agent, time_s):
        """Uçmuş füzeleri kaydet; atılmamış slotlarda yalnız CSV başlığı bulunsun."""
        name, team = agent.conf.agent_name, agent.conf.team
        for ammo_id, missile in agent.ammo.items():
            prefix = f"{key}__missile__{ammo_id}"
            filename = f"{MISSILE_MODEL} ({name}--{key}--{ammo_id}) [{team}].csv"
            extra = EXTRA_COLUMNS + MISSILE_COLUMNS
            columns = self._columns(prefix, missile, extra)
            self.objects.setdefault(filename, dict(
                file=filename, kind="missile", key=key, carrier=key, ammo_id=ammo_id,
                name=name, team=team, model=MISSILE_MODEL, columns=columns))
            self._writer(filename, columns)

            if prefix in self.finished_missiles:
                continue
            if not (missile.active or missile.target_hit or missile.target_lost):
                continue  # Henüz atılmadı: konumu uçuş ölçümü olarak yazılmaz.

            row = self._row(prefix, missile, extra, time_s)
            self.writers[filename].writerow(row)
            self.files[filename].flush()

            # Uçuşu sonlandıran BVRGym'dir. Kaydedici son durumu bir kez yazar.
            if not missile.active or not missile.alive or missile.target_hit or missile.target_lost:
                self.finished_missiles.add(prefix)

    # =========================================================================
    # C++ HESAPLARI — kullanılan ölçümün zamanıyla kaydetme
    # =========================================================================

    def record_calculations(self, calculations):
        """C++'ın son gönderilen state üzerinden yaptığı hesapları yaz.

        C++ o state'i, son record() çağrısının yazdığı ölçümden okudu. Bu yüzden
        satırın zamanı o uçağın son yazılan time_s'idir: uçak CSV'sindeki satırla
        birebir aynı değer. Eski otopilot hesap göndermezse dosya açılmaz.
        """
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
            # bool CSV'de 1/0; diğer değerler C++'tan geldiği gibi.
            row.update({key: int(value) if isinstance(value, bool) else value
                        for key, value in item.items()})
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
    # MANİFEST — senaryo, sütun açıklamaları, nesneler ve simülasyon ayarları
    # =========================================================================

    def _read_simulation(self, agent):
        aircraft = agent.simObj.aircraft_simulation_config
        missile = next(iter(agent.ammo.values()), None)
        self.simulation = dict(
            engine="JSBSim",
            physics_dt_s=agent.simObj.fdm.get_delta_t(),
            aircraft_sim_time_step=aircraft.Sim_time_step,
            aircraft_control_time_step=aircraft.Control_time_step,
            missile_sim_time_step=None if missile is None else missile.missile_simulation_config.Sim_time_step,
            missile_control_time_step=None if missile is None else missile.missile_simulation_config.Control_time_step,
        )

    def _write_manifest(self):
        columns = {TIME_COLUMN[0]: dict(
            source="record() çağrısındaki taşıyıcı saati", unit=TIME_COLUMN[1],
            note="Füze dosyalarında da taşıyıcı uçağın saatidir")}
        for column, name, unit, scale in POSE_COLUMNS + EXTRA_COLUMNS + MISSILE_COLUMNS:
            columns[column] = dict(source=name, unit=unit, scale=scale)
        columns["lat_gc_deg"]["note"] = ("BVRGym'in iç referansı; scenario.yaml'daki "
                                         "spawn enlemi bu sütunla eşleşir")
        columns["missile_time_s"]["note"] = "Füzenin kendi saati; atışta sıfırlanır"
        for column, unit in COMMAND_COLUMNS:
            columns[column] = dict(source="dış uçuş komutu", unit=unit)
        manifest = dict(
            schema_version=SCHEMA_VERSION,
            kind="bvrgym-bvr-recording",
            created_at=self.created_at,
            snapshots=self.snapshots,
            events=self.event_count,
            coordinates=dict(
                latitude_reference="WGS84 geodetic",
                longitude_reference="WGS84",
                altitude_reference="WGS84 ellipsoid height",
                note="lat_deg jeodezik enlemdir (position/lat-geod-deg), alt_ft "
                     "elipsoid yüksekliğidir (position/h-sl-ft). BVRGym içeride jeosantrik "
                     "çalışır: spawn ic/lat-gc-deg'e yazılır, o değer lat_gc_deg "
                     "sütunundadır. Koordinat referansına dönüşüm uygulanmaz; "
                     "yalnız sütun birimleri ft/kt olarak yazılır.",
            ),
            conversions="irtifa ft; hız kt; C++ mesafeleri NM; NED hızları ft/s → kt; komut irtifası m → ft",
            columns=columns,
            simulation=self.simulation,
            scenario=self.scenario,
            objects=list(self.objects.values()),
            files=dict(events="events.jsonl", calculations=CALCULATIONS_FILE),
            calculations=dict(
                rows=self.calculation_rows,
                source="C++ otopilot (command mesajının calculations alanı)",
                units=dict(distance="NM", speed="kt", angle="deg", time="s"),
                note="Satır başına bir uçağın karşı uçağa göre hesabı. time_s, C++'ın "
                     "kullandığı ölçümün zamanıdır ve uçak CSV'sindeki satırla aynıdır. "
                     "Birim sütun adının sonundadır; is_self_in_enemy_wez 1/0. Son ölçümün hesabı yoktur: "
                     "simülasyon bittiği için C++'a gönderilmez.",
            ),
        )
        path = self.directory / "manifest.json"
        path.write_text(json.dumps(manifest, ensure_ascii=False, indent=2), encoding="utf-8")
