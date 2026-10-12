"""BVR için gereken ham JSBSim/BVRGym verilerini hesap yapmadan kaydet.

Üç dosya üretilir:
  <nesne>.csv    Konum/yönelim + Tacview karşılığı olmayan ham BVR alanları.
  events.jsonl   Hedef ataması, fırlatma, isabet, kayıp, sağlık değişimi.
  manifest.json  Senaryo, birimler, koordinat referansı, sütun kaynakları, sürümler.

Birim dönüşümü, türetme ve hesaplama yapılmaz. Alan modelde yoksa sütun açılmaz;
sıfır değer uydurulmaz. Sütun adları boşluk içermez, birimi adında taşır.
"""
import csv
import json
import re
from datetime import datetime
from pathlib import Path

SCHEMA_VERSION = 2

# Zaman sütunu modelden okunmaz; record() çağrısına verilen taşıyıcı saatidir.
TIME_COLUMN = ("time_s", "s")

# Her nesnede bulunan konum ve yönelim. Enlem WGS84 jeodeziktir: JSBSim'in
# position/lat-geod-deg'i, position/h-sl-meters ise elipsoid yüksekliğidir
# (geod-alt ile farkı ölçüldü: 1,3 cm'den küçük). Boylamda jeosantrik ile
# jeodezik aynı değerdir, dönüşüm gerekmez.
POSE_COLUMNS = (
    ("lat_deg", "position/lat-geod-deg", "deg"),
    ("lon_deg", "position/long-gc-deg", "deg"),
    ("alt_m", "position/h-sl-meters", "m"),
    ("roll_deg", "attitude/phi-deg", "deg"),
    ("pitch_deg", "attitude/theta-deg", "deg"),
    ("yaw_deg", "attitude/psi-deg", "deg"),
    ("mach", "velocities/mach", "-"),
)

# BVR kararı ve değerlendirmesi için gereken ek ham alanlar. Modelde okunabilir
# olmayan atlanır. lat_gc_deg BVRGym'in iç referansıdır: spawn ic/lat-gc-deg'e
# yazıldığı için scenario.yaml'daki enlemle eşleşen sütun budur, lat_deg değil.
# Kuvvet/moment toplamları, atalet, atmosfer, rüzgâr, yer teması, kontrol yüzeyi
# konumları, gövde hızları, açısal hızlar ve alpha/beta bilinçli olarak dışarıda:
# ne sahnede çizilir ne BVR kararına girer. Gerekirse buraya eklenir.
EXTRA_COLUMNS = (
    ("lat_gc_deg", "position/lat-gc-deg", "deg"),
    ("tas_fps", "velocities/vt-fps", "ft/s"),
    ("vel_north_fps", "velocities/v-north-fps", "ft/s"),
    ("vel_east_fps", "velocities/v-east-fps", "ft/s"),
    ("vel_down_fps", "velocities/v-down-fps", "ft/s"),
    ("load_factor_g", "forces/load-factor", "-"),
    ("fuel_lb", "propulsion/total-fuel-lbs", "lb"),
)

# Füzenin kendi saati. time_s taşıyıcının saatidir; bu füzenin kendisidir.
MISSILE_COLUMNS = (("missile_time_s", "simulation/sim-time-sec", "s"),)

# Dışarıdan verilen uçuş hedefi. Ölçüm değil, bizim komutumuz; olgu olarak
# yazılır. JSBSim'in fcs/throttle-cmd-norm'u ayrı sütun değildir: uçakta
# set_throttle() doğrudan cmd_throttle_norm'u yazar, füzede autopilot sabit
# 0,7 yazar — iki durumda da bilgi taşımaz.
COMMAND_COLUMNS = (
    ("cmd_heading_deg", "deg"),
    ("cmd_alt_m", "m"),
    ("cmd_throttle_norm", "-"),
)

MISSILE_MODEL = "AIM-120 AMRAAM"
AIRCRAFT_MODEL = "F-16"


class Recorder:
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

    # ------------------------------------------------------------------ CSV

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
        """Modelde gerçekten okunabilen (sütun, property) çiftlerini bir kez belirle."""
        if prefix not in self.properties:
            readable = set()
            for entry in model.fdm.get_property_catalog():
                name, permission = entry.rsplit(" (", 1)
                if "R" in permission:
                    readable.add(name)
            self.properties[prefix] = [(column, name) for column, name, _ in extra
                                       if name in readable]
        return self.properties[prefix]

    def _columns(self, prefix, model, extra, command=False):
        columns = [TIME_COLUMN[0]]
        columns += [column for column, _, _ in POSE_COLUMNS]
        columns += [column for column, _ in self._readable(prefix, model, extra)]
        if command:
            columns += [column for column, _ in COMMAND_COLUMNS]
        return columns

    def _row(self, prefix, model, extra, time_s, command=None):
        values = {TIME_COLUMN[0]: time_s}
        for column, name, _ in POSE_COLUMNS:
            values[column] = model.fdm[name]
        for column, name in self._readable(prefix, model, extra):
            values[column] = model.fdm[name]
        if command is not None:
            values.update(command)
        return values

    # --------------------------------------------------------------- olaylar

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

    # ----------------------------------------------------------------- kayıt

    def record(self, agents, commands=None):
        """Her çağrı bir snapshot yazar; bütün fizik alt adımları değildir."""
        self.snapshots += 1
        for key, agent in agents.items():
            name, team = agent.conf.agent_name, agent.conf.team
            time_s = agent.simObj.fdm["simulation/sim-time-sec"]
            if self.simulation is None:
                self._read_simulation(agent)
            self._detect_events(key, agent, time_s)

            command = None
            if commands is not None and key in commands:
                command = dict(zip((column for column, _ in COMMAND_COLUMNS),
                                   commands[key]))
            prefix = f"{key}__aircraft"
            filename = f"{AIRCRAFT_MODEL} ({name}--{key}) [{team}].csv"
            columns = self._columns(prefix, agent.simObj, EXTRA_COLUMNS, command=True)
            self.objects.setdefault(filename, dict(
                file=filename, kind="aircraft", key=key, name=name, team=team,
                model=AIRCRAFT_MODEL, ammo=len(agent.ammo), columns=columns))
            row = self._row(prefix, agent.simObj, EXTRA_COLUMNS, time_s, command)
            self._writer(filename, columns).writerow(row)
            self.files[filename].flush()

            for ammo_id, missile in agent.ammo.items():
                prefix = f"{key}__missile__{ammo_id}"
                filename = f"{MISSILE_MODEL} ({name}--{key}--{ammo_id}) [{team}].csv"
                extra = EXTRA_COLUMNS + MISSILE_COLUMNS
                columns = self._columns(prefix, missile, extra)
                self.objects.setdefault(filename, dict(
                    file=filename, kind="missile", key=key, carrier=key, ammo_id=ammo_id,
                    name=name, team=team, model=MISSILE_MODEL, columns=columns))
                self._writer(filename, columns)  # Atılmasa da başlıkla hazır dursun.
                if prefix in self.finished_missiles:
                    continue
                if not (missile.active or missile.target_hit or missile.target_lost):
                    continue  # Henüz atılmadı: fizik durumu uçan füze ölçümü değil.
                row = self._row(prefix, missile, extra, time_s)
                self.writers[filename].writerow(row)
                self.files[filename].flush()
                if not missile.active or not missile.alive or missile.target_hit or missile.target_lost:
                    self.finished_missiles.add(prefix)  # Bitiş satırı bir kez yazılır.

    # -------------------------------------------------------------- manifest

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
        for column, name, unit in POSE_COLUMNS + EXTRA_COLUMNS + MISSILE_COLUMNS:
            columns[column] = dict(source=name, unit=unit)
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
                note="lat_deg jeodezik enlemdir (position/lat-geod-deg), alt_m "
                     "elipsoid yüksekliğidir (position/h-sl-meters; geod-alt ile "
                     "farkı 1,3 cm'den küçük ölçüldü). BVRGym içeride jeosantrik "
                     "çalışır: spawn ic/lat-gc-deg'e yazılır, o değer lat_gc_deg "
                     "sütunundadır. Kayıtta dönüşüm yapılmaz, iki referans da "
                     "JSBSim'den ham okunur.",
            ),
            conversions="yok; bütün değerler kaynak property biriminde",
            columns=columns,
            simulation=self.simulation,
            scenario=self.scenario,
            objects=list(self.objects.values()),
            files=dict(events="events.jsonl"),
        )
        path = self.directory / "manifest.json"
        path.write_text(json.dumps(manifest, ensure_ascii=False, indent=2), encoding="utf-8")


# ============================================================================
# UÇAK → F-16 (<isim>--<ajan>) [<takım>].csv
# ============================================================================
# time_s, lat_deg, lon_deg, alt_m, roll_deg, pitch_deg, yaw_deg, mach ve
# ardından BVR alanları: lat_gc_deg, tas_fps, vel_north/east/down_fps,
# load_factor_g, fuel_lb. Değerler ham property değerleridir; sütun adı birimi
# söyler, dönüşüm yapılmaz. En sonda dışarıdan verilen uçuş komutu
# (cmd_heading_deg, cmd_alt_m, cmd_throttle_norm) bulunur. Komut sütunları
# ölçüm değildir; o snapshot'ta uygulanan hedeftir ve gerçekleşen uçuşla
# karşılaştırmak içindir.
#
# ============================================================================
# FÜZE → AIM-120 AMRAAM (<isim>--<ajan>--<mühimmat>) [<takım>].csv
# ============================================================================
# Dosya başlangıçta başlıkla açılır; veri satırları yalnızca atıştan sonra yazılır.
# time_s taşıyıcının saatidir, missile_time_s füzenin kendi saatidir.
# Bitiş durumu bir kez yazılır, ardından bu füzenin kaydı durur.
#
# ============================================================================
# OLAYLAR → events.jsonl
# ============================================================================
# target_assigned, target_cleared, missile_launched, missile_hit, missile_lost,
# health_changed. Satır başına bir JSON nesnesi; seq sıralama içindir.
# sim_time_s, değişimin görüldüğü snapshot'ın zamanıdır — fizik anı değil.
# Çözünürlük record() aralığı kadardır. Olaylar BVRGym bayraklarının
# değişiminden okunur; mesafe veya isabet kararı burada hesaplanmaz.
#
# ============================================================================
# MANİFEST → manifest.json
# ============================================================================
# Senaryo, sütun kaynak/birim listesi, koordinat referansı, fizik adımı ve
# nesne listesi. Koordinat referansının açıkça yazılması, okuyucunun sütunu
# yanlış referansta çözmesini önler.
