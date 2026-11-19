"""Dünya: uçaklar, füzeler, saat. Her uçağa kendi paketini üretir, komut ve atış emrini uygular, olayları toplar.

TCP, kayıt ve taktik karar burada yoktur. Mesaj biçimi: docs/mimari/MESAJ_PROTOKOLU_V2.md.
"""
import math

from simulator.aircraft import Aircraft
from simulator.flight_control import FlightCommand
from simulator.flight_control.aml import AMLController
from simulator.physics import earth
from simulator.physics.jsbsim_model import PHYSICS_DT_S
from simulator.scenario import validate_scenario

# g_limit kabul aralığı (tel doğrulaması); eski src/sim/simulation.py ile aynı. Uçağın asıl sınırını FBW ve C++ profili uygular.
G_LIMIT_MIN, G_LIMIT_MAX = 1.5, 9.0
DIRECT_G_MIN, DIRECT_G_MAX = -4.0, 9.0   # doğrudan mod g aralığı: f16.xml FBW "+9 G, −4 G"
LOCKED_MODES = ("datalink", "seeker")


def missile_id(owner_key, slot):
    return f"{owner_key}/missile/{slot}"


class Simulation:
    def __init__(self, scenario, controller_factory=AMLController):
        validate_scenario(scenario)
        self.world = scenario["world"]
        self.tick = 0
        self.aircraft = {key: Aircraft(key, settings, controller_factory)
                         for key, settings in scenario["agents"].items()}
        self.events = {key: [] for key in self.aircraft}   # son step'te olanlar; bir sonraki state'te gider
        self.start_time_s = self.time_s
        self.sensors = self.world.get("sensors", False)    # kapalı: düşman hep görünür, RWR boş (scenario.yaml)
        for plane in self.aircraft.values():
            plane.radar.realistic = self.sensors
        self._scan_radars()                                # ilk state'te contacts dolu olsun

    @property
    def time_s(self):
        return next(iter(self.aircraft.values())).model.time_s   # bütün modeller aynı dt ile birlikte ilerler

    def finished(self):
        return self.time_s - self.start_time_s >= self.world["duration_s"]

    # =========================================================================
    # MESAJLAR — simülasyonu ilerletmez
    # =========================================================================

    def hello(self):
        """Başlangıçta bir kez: zamanlama, uçaklar ve slot → füze profili."""
        return dict(
            physics_dt_s=PHYSICS_DT_S,
            tick_s=PHYSICS_DT_S * self.world["physics_steps_per_tick"],
            aircraft=[dict(id=p.key, name=p.name, type=p.type, team=p.team,
                           weapons=[dict(slot=m.slot, profile=m.profile.profile, designation=m.profile.designation)
                                    for m in p.missiles])
                      for p in self.aircraft.values()])

    def read_state(self):
        return dict(time_s=self.time_s, aircraft={key: self.packet(key) for key in self.aircraft})

    def packet(self, key):
        """Bir uçağın gerçekte bilebileceği: kendisi, füzeleri, gördükleri, uyarılar, olaylar."""
        plane = self.aircraft[key]
        command = plane.command
        own = dict(id=plane.key, name=plane.name, type=plane.type, team=plane.team, health=plane.health,
                   kinematics=plane.model.kinematics(), load_factor_g=plane.model.load_factor,
                   fuel_kg=plane.model.fuel_kg,
                   applied_command=dict(mode=command.mode, heading_deg=command.heading_deg,
                                        altitude_m=command.altitude_m, throttle=command.throttle,
                                        g_limit=command.g_limit, bank_deg=command.bank_deg, g=command.g))
        weapons = [self._weapon(plane, missile) for missile in plane.missiles]
        others = [other for other in self.aircraft.values() if other is not plane]
        contacts = plane.tracker.contacts(self.aircraft, self.time_s)                # radar iz dosyası (v2.1)
        rwr = [dict(emitter_id=other.key, bearing_deg=self._bearing_deg(plane, other))
               for other in others if self.sensors and other.radar.sees(other, plane)]   # beni gören radarlar; sensörler kapalıyken boş
        warnings = [dict(missile_id=missile_id(other.key, m.slot), owner_id=other.key,
                         designation=m.profile.designation, source="truth", flight_time_s=m.time_s,
                         is_active=m.seeker.mode == "seeker", kinematics=m.kinematics())
                    for other in others for m in other.missiles
                    if m.state == "in_flight" and m.target_key == key]
        return dict(own=own, rwr=rwr, weapons=weapons, contacts=contacts, warnings=warnings,
                    events=list(self.events[key]))

    @staticmethod
    def _bearing_deg(own, other):
        """Diğer uçağın gerçek kerterizi (kuzeyden, 0..360)."""
        north, east, _ = earth.ecef_to_ned(other.position_ecef() - own.position_ecef(),
                                           own.model.lat_deg, own.model.lon_deg)
        return math.degrees(math.atan2(east, north)) % 360.0

    def _scan_radars(self):
        """Her uçağın radarı diğerlerini tarar; iz dosyası ve radar olayları güncellenir."""
        for plane in self.aircraft.values():
            others = [other for other in self.aircraft.values() if other is not plane]
            self.events[plane.key] += plane.tracker.update(plane, others, self.time_s)

    def read_flight_commands(self):
        """Uygulanan komutlar, command mesajındaki biçimde (testler ve araçlar için)."""
        result = []
        for plane in self.aircraft.values():
            item = dict(id=plane.key, heading_deg=plane.command.heading_deg,
                        altitude_m=plane.command.altitude_m, throttle=plane.command.throttle)
            if plane.command.g_limit is not None:
                item["g_limit"] = plane.command.g_limit
            result.append(item)
        return result

    @staticmethod
    def _weapon(plane, missile):
        ready = missile.state == "ready"
        return dict(slot=missile.slot, id=missile_id(plane.key, missile.slot),
                    designation=missile.profile.designation, state=missile.state,
                    request_id=missile.request_id, target_id=missile.target_key,
                    flight_time_s=None if ready else missile.time_s,
                    kinematics=None if ready else missile.kinematics(),
                    guidance_mode=missile.seeker.mode, is_locked=missile.seeker.mode in LOCKED_MODES,
                    is_pitbull=missile.seeker.is_pitbull, outcome=missile.outcome)

    # =========================================================================
    # KOMUT VE İLERLETME
    # =========================================================================

    def step(self, aircraft_commands, launches=()):
        """Bir tick: komutları doğrula, atış emirlerini uygula, physics_steps_per_tick fizik adımı ilerlet."""
        if self.finished():
            raise ValueError("Simülasyon süresi doldu.")
        commands = self._validate_flight_commands(aircraft_commands)   # hepsi doğrulanmadan hiçbiri uygulanmaz
        self.events = {key: [] for key in self.aircraft}
        for order in launches:
            self.fire(order["id"], order["target_id"], order.get("request_id"))
        for key, command in commands.items():
            self.aircraft[key].command = command
        for _ in range(self.world["physics_steps_per_tick"]):
            for plane in self.aircraft.values():
                plane.run()
            for plane in self.aircraft.values():
                for missile in plane.missiles:
                    if missile.state == "in_flight":
                        self._step_missile(plane, missile)
        self._scan_radars()                                # tick başına bir tarama
        self.tick += 1

    def fire(self, shooter_id, target_id, request_id=None):
        """Hazır ilk füzeyi hedefe at → True; olmazsa launch_rejected olayı ve False.
        Havada başka füze olması engel değil (C++ açık atış emri verir)."""
        events = self.events.get(shooter_id)
        if shooter_id == target_id or shooter_id not in self.aircraft or target_id not in self.aircraft:
            if events is not None:
                events.append(dict(type="launch_rejected", request_id=request_id, reason="invalid_target"))
            return False
        shooter = self.aircraft[shooter_id]
        missile = next((m for m in shooter.missiles if m.state == "ready"), None)
        if missile is None:
            events.append(dict(type="launch_rejected", request_id=request_id, reason="no_ready_missile"))
            return False
        missile.launch(shooter.model, target_id)
        missile.request_id = request_id
        events.append(dict(type="launch_accepted", request_id=request_id, slot=missile.slot, target_id=target_id))
        self.events[target_id].append(dict(type="missile_launch_warning",
                                           missile_id=missile_id(shooter_id, missile.slot), owner_id=shooter_id))
        return True

    def _step_missile(self, plane, missile):
        """Füzeyi bir fizik adımı ilerlet; durum değişimlerini olay olarak yaz."""
        target = self.aircraft[missile.target_key]
        was_pitbull, was_locked = missile.seeker.is_pitbull, missile.seeker.mode in LOCKED_MODES
        first_step = missile.time_s == 0.0                    # atış adımında ilk kilit olay sayılmaz
        missile.step(PHYSICS_DT_S, plane, target)
        own, ident = self.events[plane.key], missile_id(plane.key, missile.slot)
        if missile.seeker.is_pitbull and not was_pitbull:
            own.append(dict(type="missile_pitbull", slot=missile.slot))
            self.events[target.key].append(dict(type="missile_active_warning", missile_id=ident))
        if missile.state == "ended":
            own.append(dict(type="missile_hit" if missile.outcome == "hit" else "missile_lost",
                            slot=missile.slot, target_id=target.key))
            if missile.outcome == "hit":
                self.events[target.key].append(dict(type="hit_received", missile_id=ident, health=target.health))
            return
        locked = missile.seeker.mode in LOCKED_MODES
        if locked != was_locked and not first_step:
            own.append(dict(type="missile_lock_regained" if locked else "missile_lock_lost",
                            slot=missile.slot, guidance_mode=missile.seeker.mode))

    def _validate_flight_commands(self, aircraft_commands):
        if not isinstance(aircraft_commands, list):
            raise ValueError("aircraft komutları liste olmalı.")
        commands = {}
        for item in aircraft_commands:
            if not isinstance(item, dict) or not isinstance(item.get("id"), str):
                raise ValueError("Her komutta uçak id alanı bulunmalı.")
            key = item["id"]
            if key not in self.aircraft or key in commands:
                raise ValueError(f"Bilinmeyen veya tekrarlanan uçak id: {key}")
            mode = item.get("mode", "target")
            if mode == "target":
                commands[key] = self._target_command(key, item)
            elif mode == "direct":
                commands[key] = self._direct_command(key, item)
            else:
                raise ValueError(f"{key}: mode 'target' ya da 'direct' olmalı.")
        if set(commands) != set(self.aircraft):
            raise ValueError("Her uçak için tam bir komut gerekli.")
        return commands

    @staticmethod
    def _number(key, item, name, low, high):
        value = item.get(name)
        if type(value) not in (int, float) or not math.isfinite(value):
            raise ValueError(f"{key}: {name} sonlu sayı olmalı.")
        if not low <= value <= high:
            raise ValueError(f"{key}: {name} {low}..{high} aralığında olmalı.")
        return float(value)

    def _target_command(self, key, item):
        heading = self._number(key, item, "heading_deg", 0.0, 360.0)
        altitude = self._number(key, item, "altitude_m", -math.inf, math.inf)
        throttle = self._number(key, item, "throttle", 0.0, 1.0)
        g_limit = None
        if item.get("g_limit") is not None:
            g_limit = self._number(key, item, "g_limit", G_LIMIT_MIN, G_LIMIT_MAX)
        return FlightCommand(heading, altitude, throttle, g_limit)

    def _direct_command(self, key, item):
        """Doğrudan mod: yatış + g + gaz. Yön/irtifa son hedef modundan korunur (kayıt ve geri dönüş için)."""
        previous = self.aircraft[key].command
        return FlightCommand(previous.heading_deg, previous.altitude_m,
                             self._number(key, item, "throttle", 0.0, 1.0), None, mode="direct",
                             bank_deg=self._number(key, item, "bank_deg", -180.0, 180.0),
                             g=self._number(key, item, "g", DIRECT_G_MIN, DIRECT_G_MAX))
