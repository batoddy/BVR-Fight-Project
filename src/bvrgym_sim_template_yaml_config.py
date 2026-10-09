"""YAML'dan dünya ve ajan oluştur; uçuş komutlarını Python'dan ver."""
import copy
import os
import sys
from pathlib import Path
from types import ModuleType, SimpleNamespace

import yaml
from recorder import CSVRecorder

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


def create_agent(settings):
    aircraft = copy_config(f16_config)
    missile = copy_config(AAM_config)
    aircraft.aircraft_simulation.Sim_time_step = 1
    missile.missile_simulation.Sim_time_step = 1
    return BaseBVRAgent(SimpleNamespace(
        agent_name=settings["name"], team=settings["team"], aircraft_name="F-16",
        agent_parameters=Agent_parameters(ammo=settings["ammo"], **settings["spawn"]),
        aircraft_simObj_conf=aircraft,
        missile_simObj_conf=missile,
    ), None)

# 1. YAML'da tanımlı bütün ajanları oluştur.
agents = {key: create_agent(settings)
          for key, settings in scenario["agents"].items()}
if not agents:
    raise ValueError("scenario.yaml içinde en az bir ajan tanımlanmalı.")
blue, red = agents["blue"], agents["red"]

# 2. Komutlar: [hedef yön (derece), hedef irtifa (metre), gaz].
# Başlangıç yönünü ve irtifasını koru. Kendi uçuş komutlarını burada yazabilirsin.
commands = {key: [settings["spawn"]["heading"], settings["spawn"]["alt"], 0.49]
            for key, settings in scenario["agents"].items()}

# İstersen hedef seçip ateşle; otomatik atış yok.
# blue.set_target(red)
# blue.launch_missile()

# 3. Bütün ajanları ilerlet. Dış komutlarını bu döngüde uygulayabilirsin.
clock = next(iter(agents.values())).simObj
start_time = clock.get_sim_time_sec()
with CSVRecorder(WORKSPACE / "recordings") as recorder:
    print("Arayüz kayıt klasörü:", recorder.directory)
    recorder.record(agents)  # Başlangıç durumunu da kaydet.
    while clock.get_sim_time_sec() - start_time < world["duration_s"]:
        for key, agent in agents.items():
            agent.apply_action(commands[key])

        recorder.record(agents)
        # Veri okuma örneği: altitude_m = blue.simObj.get_altitude()
