"""YAML'dan dünya ve ajan oluştur; uçuş komutlarını Python'dan ver."""
import copy
import os
import sys
import time
from pathlib import Path
from types import ModuleType, SimpleNamespace

import yaml

WORKSPACE = Path(__file__).resolve().parents[1]
BVRGYM = WORKSPACE / "BVRGym"
with (WORKSPACE / "scenario.yaml").open(encoding="utf-8-sig") as file:
    scenario = yaml.safe_load(file)
world = scenario["world"]

sys.path.insert(0, str(BVRGYM))
os.chdir(BVRGYM)  # BVRGym'in göreli XML/model yolları için.

from data_classes.agent_dataclass import Agent_parameters
from jsb_gym.agents.agents import BaseBVRAgent
from jsb_gym.simObjects.config import f16_config, AAM_config


def copy_config(module):
    # Ajanların ayarları ve kontrolcü durumları birbirinden bağımsız olsun.
    return SimpleNamespace(**{
        key: copy.deepcopy(value) for key, value in vars(module).items()
        if not key.startswith("_") and not isinstance(value, ModuleType)
    })


def apply_world(agent):
    # JSBSim reset sırasında ortamı sıfırlayabilir; rüzgârı tekrar uygula.
    for model in [agent.simObj, *agent.ammo.values()]:
        for direction in ("north", "east", "down"):
            speed = world["wind_mps"][direction]
            model.fdm[f"atmosphere/wind-{direction}-fps"] = speed / 0.3048


def create_agent(settings):
    aircraft = copy_config(f16_config)
    missile = copy_config(AAM_config)
    aircraft.aircraft_simulation.Sim_time_step = 1
    missile.missile_simulation.Sim_time_step = 1
    agent = BaseBVRAgent(SimpleNamespace(
        agent_name=settings["name"], team=settings["team"], aircraft_name="F-16",
        agent_parameters=Agent_parameters(ammo=settings["ammo"], **settings["spawn"]),
        aircraft_simObj_conf=aircraft,
        missile_simObj_conf=missile,
    ), None)

    apply_world(agent)  # YAML m/s → JSBSim ft/s.
    return agent


# 1. YAML'da tanımlı bütün ajanları oluştur.
agents = {key: create_agent(settings)
          for key, settings in scenario["agents"].items()}
if not agents:
    raise ValueError("scenario.yaml içinde en az bir ajan tanımlanmalı.")
blue, red = agents["blue"], agents["red"]

# 2. Komutlar: [hedef yön (derece), hedef irtifa (metre), gaz].
# Yeni ajanlar başlangıç yönünü/irtifasını korur; komutunu burada değiştir.
commands = {key: [agent.simObj.get_psi(), agent.simObj.get_altitude(), 0.49]
            for key, agent in agents.items()}
commands["blue"] = [0, 5000, 0.99]
commands["red"] = [180, 7000, 0.99]

# İstersen hedef seçip ateşle; otomatik atış yok.
# blue.set_target(red)
# blue.launch_missile()

# 3. Bütün ajanları ilerlet. Dış komutlarını bu döngüde uygulayabilirsin.
clock = next(iter(agents.values())).simObj
start_time = clock.get_sim_time_sec()
while clock.get_sim_time_sec() - start_time < world["duration_s"]:
    for key, agent in agents.items():
        apply_world(agent)
        agent.apply_action(commands[key])

    # Veri okuma örneği: altitude_m = blue.simObj.get_altitude()
    time.sleep(world["sleep_s"])
