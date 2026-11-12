"""Dünya: uçaklar, füzeler, saat. Ölçüm okur, uçuş komutu uygular, atış emri yürütür.

TCP, kayıt ve taktik karar burada yoktur. Mesaj biçimi eski src/sim ile aynıdır (C++ değişmez).
"""
import math

from simulator.aircraft import Aircraft
from simulator.flight_control import FlightCommand
from simulator.flight_control.aml import AMLController
from simulator.physics.jsbsim_model import PHYSICS_DT_S
from simulator.scenario import validate_scenario

# g_limit kabul aralığı (tel doğrulaması); eski src/sim/simulation.py ile aynı. Uçağın asıl sınırını FBW ve C++ profili uygular.
G_LIMIT_MIN, G_LIMIT_MAX = 1.5, 9.0


class Simulation:
    def __init__(self, scenario, controller_factory=AMLController):
        validate_scenario(scenario)
        self.world = scenario["world"]
        self.tick = 0
        self.aircraft = {key: Aircraft(key, settings, controller_factory)
                         for key, settings in scenario["agents"].items()}
        self.start_time_s = self.time_s

    @property
    def time_s(self):
        return next(iter(self.aircraft.values())).model.time_s   # bütün modeller aynı dt ile birlikte ilerler

    def finished(self):
        return self.time_s - self.start_time_s >= self.world["duration_s"]

    # =========================================================================
    # ÖLÇÜM — simülasyonu ilerletmez
    # =========================================================================

    def read_state(self):
        aircraft = [plane.state() for plane in self.aircraft.values()]
        missiles = []
        for plane in self.aircraft.values():
            for missile in plane.missiles:
                ready = missile.state == "ready"
                missiles.append(dict(id=f"{plane.key}/missile/{missile.slot}", owner_id=plane.key,
                                     target_id=missile.target_key, state=missile.state, outcome=missile.outcome,
                                     flight_time_s=None if ready else missile.time_s,
                                     kinematics=None if ready else missile.kinematics()))
        return dict(tick=self.tick, sim_time_s=self.time_s, aircraft=aircraft, missiles=missiles,
                    flight_commands=self.read_flight_commands())

    def read_flight_commands(self):
        result = []
        for plane in self.aircraft.values():
            command = plane.command
            item = dict(id=plane.key, heading_deg=command.heading_deg,
                        altitude_m=command.altitude_m, throttle=command.throttle)
            if command.g_limit is not None:
                item["g_limit"] = command.g_limit
            result.append(item)
        return result

    # =========================================================================
    # KOMUT VE İLERLETME
    # =========================================================================

    def step(self, aircraft_commands):
        """Komutları doğrula, sonra bir tick (physics_steps_per_tick fizik adımı) ilerlet."""
        if self.finished():
            raise ValueError("Simülasyon süresi doldu.")
        commands = self._validate_flight_commands(aircraft_commands)   # hepsi doğrulanmadan hiçbiri uygulanmaz
        for key, command in commands.items():
            self.aircraft[key].command = command
        for _ in range(self.world["physics_steps_per_tick"]):
            for plane in self.aircraft.values():
                plane.run()
            for plane in self.aircraft.values():
                for missile in plane.missiles:
                    if missile.state == "in_flight":
                        missile.step(PHYSICS_DT_S, plane, self.aircraft[missile.target_key])
        self.tick += 1

    def fire(self, shooter_id, target_id):
        """Hazır ilk füzeyi hedefe at → True; hazır füze yoksa ya da hedef geçersizse False.
        Havada başka füze olması engel değil (C++ açık atış emri verir)."""
        if shooter_id == target_id or shooter_id not in self.aircraft or target_id not in self.aircraft:
            return False
        shooter = self.aircraft[shooter_id]
        missile = next((m for m in shooter.missiles if m.state == "ready"), None)
        if missile is None:
            return False
        missile.launch(shooter.model, target_id)
        return True

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
            values = [item.get("heading_deg"), item.get("altitude_m"), item.get("throttle")]
            if any(type(value) not in (int, float) or not math.isfinite(value) for value in values):
                raise ValueError(f"{key}: heading, altitude ve throttle sonlu sayı olmalı.")
            if not 0 <= values[0] <= 360 or not 0 <= values[2] <= 1:
                raise ValueError(f"{key}: heading 0..360, throttle 0..1 aralığında olmalı.")
            g_limit = item.get("g_limit")
            if g_limit is not None:
                if type(g_limit) not in (int, float) or not math.isfinite(g_limit):
                    raise ValueError(f"{key}: g_limit sonlu sayı olmalı.")
                if not G_LIMIT_MIN <= g_limit <= G_LIMIT_MAX:
                    raise ValueError(f"{key}: g_limit {G_LIMIT_MIN}..{G_LIMIT_MAX} aralığında olmalı.")
            commands[key] = FlightCommand(*values, g_limit)
        if set(commands) != set(self.aircraft):
            raise ValueError("Her uçak için tam bir komut gerekli.")
        return commands
