"""BVRGym ajanlarını oluştur, ölçümlerini oku ve verilen komutla ilerlet.

TCP, kayıt, bekleme veya otomatik taktik karar bu sınıfta bulunmaz.
"""
import copy
import math
import os
import sys
from pathlib import Path
from types import ModuleType, SimpleNamespace


class Simulation:
    def __init__(self, scenario, bvrgym_path):
        self.world = scenario["world"]
        self.tick = 0
        step = self.world["sim_time_step"]
        if type(step) is not int or step < 1:
            raise ValueError("world.sim_time_step pozitif tam sayı olmalı.")
        for key in ("duration_s", "sleep_s"):
            value = self.world[key]
            if type(value) not in (int, float) or not math.isfinite(value) or value < 0:
                raise ValueError(f"world.{key} sonlu ve negatif olmayan sayı olmalı.")
        if not scenario["agents"]:
            raise ValueError("Senaryoda en az bir ajan bulunmalı.")

        # BVRGym XML/model dosyalarını kendi klasörüne göreli açar.
        # Oluşturma bittikten sonra çağıranın çalışma dizinini geri getir.
        bvrgym_path = Path(bvrgym_path).resolve()
        if not bvrgym_path.is_dir():
            raise FileNotFoundError(f"BVRGym klasörü bulunamadı: {bvrgym_path}")
        if str(bvrgym_path) not in sys.path:
            sys.path.insert(0, str(bvrgym_path))
        previous_cwd = Path.cwd()
        try:
            os.chdir(bvrgym_path)
            self.agents = {
                key: self._create_agent(settings)
                for key, settings in scenario["agents"].items()
            }
        finally:
            os.chdir(previous_cwd)

        self.commands = {
            key: [settings["spawn"]["heading"], settings["spawn"]["alt"], settings["throttle"]]
            for key, settings in scenario["agents"].items()
        }
        self.clock = next(iter(self.agents.values())).simObj
        self.start_time = self.clock.get_sim_time_sec()

    @staticmethod
    def _copy_config(module):
        """Her ajanın fizik ve PID ayarlarını bağımsız kopyala."""
        return SimpleNamespace(**{
            key: copy.deepcopy(value) for key, value in vars(module).items()
            if not key.startswith("_") and not isinstance(value, ModuleType)
        })

    def _create_agent(self, settings):
        """YAML tanımından bir uçak ve mühimmatını oluştur."""
        from data_classes.agent_dataclass import Agent_parameters
        from jsb_gym.agents.agents import BaseBVRAgent
        from jsb_gym.simObjects.config import f16_config, AAM_config

        aircraft = self._copy_config(f16_config)
        missile = self._copy_config(AAM_config)
        aircraft.aircraft_simulation.Sim_time_step = self.world["sim_time_step"]
        missile.missile_simulation.Sim_time_step = self.world["sim_time_step"]
        for config in (aircraft, missile):
            config.data_output_xml = None
            config.fg_sleep_time = None  # Beklemeyi yalnızca ana döngü yönetir.

        return BaseBVRAgent(SimpleNamespace(
            agent_name=settings["name"], team=settings["team"], aircraft_name="F-16",
            agent_parameters=Agent_parameters(ammo=settings["ammo"], **settings["spawn"]),
            aircraft_simObj_conf=aircraft, missile_simObj_conf=missile,
        ), None)

    def finished(self):
        return self.clock.get_sim_time_sec() - self.start_time >= self.world["duration_s"]

    def initial_commands(self):
        """Saklanan yön/irtifa/gaz hedeflerini mesajdaki liste biçimine çevir."""
        return [
            dict(id=key, heading_deg=values[0], altitude_m=values[1], throttle=values[2])
            for key, values in self.commands.items()
        ]

    @staticmethod
    def _kinematics(model):
        """BVRGym/JSBSim değerleri; mesafe veya açı hesabı yapılmaz."""
        return dict(
            lat_deg=float(model.fdm["position/lat-geod-deg"]),
            lon_deg=model.get_long_gc_deg(), alt_msl_m=model.get_altitude(),
            yaw_deg=model.get_psi(), pitch_deg=model.get_theta(), roll_deg=model.get_phi(),
            tas_mps=model.get_true_airspeed(), mach=model.get_mach(),
            velocity_ned_mps=[model.get_v_north(), model.get_v_east(), model.get_v_down()],
        )

    def read_state(self):
        """Bütün uçak ve füzeleri oku; simülasyonu ilerletme."""
        aircraft, missiles = [], []
        for key, agent in self.agents.items():
            aircraft.append(dict(
                id=key, name=agent.conf.agent_name, team=agent.conf.team,
                health=agent.healthPoints, **self._kinematics(agent.simObj),
            ))
            for ammo_id, missile in agent.ammo.items():
                ready = missile.is_ready_to_launch()
                state = "ready" if ready else "in_flight" if missile.is_active() else "ended"
                target_id = next((other_key for other_key, other in self.agents.items()
                                  if other is missile.target), None)
                missiles.append(dict(
                    id=f"{key}/missile/{ammo_id}", owner_id=key, target_id=target_id,
                    state=state,
                    outcome="hit" if missile.target_hit else "lost" if missile.target_lost else None,
                    flight_time_s=None if ready else missile.get_sim_time_sec(),
                    kinematics=None if ready else self._kinematics(missile),
                ))
        return dict(tick=self.tick, sim_time_s=self.clock.get_sim_time_sec(),
                    aircraft=aircraft, missiles=missiles,
                    flight_commands=self.initial_commands())

    def step(self, aircraft_commands):
        """Bütün uçakların komutlarını doğrula; sonra hepsini bir adım ilerlet."""
        if self.finished():
            raise ValueError("Simülasyon süresi doldu.")
        if not isinstance(aircraft_commands, list):
            raise ValueError("aircraft komutları liste olmalı.")

        commands = {}
        for item in aircraft_commands:
            if not isinstance(item, dict) or not isinstance(item.get("id"), str):
                raise ValueError("Her komutta uçak id alanı bulunmalı.")
            key = item["id"]
            if key not in self.agents or key in commands:
                raise ValueError(f"Bilinmeyen veya tekrarlanan uçak id: {key}")
            values = [item.get("heading_deg"), item.get("altitude_m"), item.get("throttle")]
            if any(type(value) not in (int, float) or not math.isfinite(value) for value in values):
                raise ValueError(f"{key}: heading, altitude ve throttle sonlu sayı olmalı.")
            if not 0 <= values[0] <= 360 or not 0 <= values[2] <= 1:
                raise ValueError(f"{key}: heading 0..360, throttle 0..1 aralığında olmalı.")
            commands[key] = values
        if set(commands) != set(self.agents):
            raise ValueError("Her uçak için tam bir komut gerekli.")

        # Doğrulama bitmeden hiçbir uçak ilerlemez.
        for key, agent in self.agents.items():
            agent.apply_action(commands[key])
        self.commands = commands
        self.tick += 1

    def fire(self, shooter_id, target_id):
        """Açık atış emri; taktik koşul veya otomatik hedef seçimi yok."""
        shooter, target = self.agents[shooter_id], self.agents[target_id]
        if shooter is target or shooter.is_own_missile_active():
            return False
        if not any(missile.is_ready_to_launch() for missile in shooter.ammo.values()):
            return False
        shooter.set_target(target)
        shooter.launch_missile()
        return True
