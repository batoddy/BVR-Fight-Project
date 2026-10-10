"""İki uçak oluştur, komut ver ve simülasyonu ilerlet."""
import copy
import os
import sys
import time
from pathlib import Path
from types import ModuleType, SimpleNamespace

BVRGYM = Path(__file__).resolve().parents[1] / "BVRGym"
sys.path.insert(0, str(BVRGYM))
os.chdir(BVRGYM)  # BVRGym'in göreli XML/model yolları için.

from data_classes.agent_dataclass import Agent_parameters
from jsb_gym.agents.agents import BaseBVRAgent
from jsb_gym.simObjects.config import f16_config, AAM_config


def copy_config(module):
    # Her ajanın ayarları ve kontrolcü durumu birbirinden bağımsız olsun.
    return SimpleNamespace(**{
        key: copy.deepcopy(value) for key, value in vars(module).items()
        if not key.startswith("_") and not isinstance(value, ModuleType)
    })


def create_agent(name, team, spawn):
    aircraft = copy_config(f16_config)
    missile = copy_config(AAM_config)
    aircraft.aircraft_simulation.Sim_time_step = 1
    missile.missile_simulation.Sim_time_step = 1
    return BaseBVRAgent(SimpleNamespace(
        agent_name=name, team=team, aircraft_name="F-16",
        agent_parameters=spawn,
        aircraft_simObj_conf=aircraft,
        missile_simObj_conf=missile,
    ), None)


# 1. Başlangıç: isim, takım, füze sayısı ve uçuş durumu.
# lat/long/heading: derece; alt: metre; vel: m/s.
blue = create_agent("KE", "Blue", Agent_parameters(
    ammo=2, lat=59.0, long=18.0, alt=8000, vel=300, heading=0,
))
red = create_agent("Bogey", "Red", Agent_parameters(
    ammo=2, lat=59.25, long=18.0, alt=3000, vel=300, heading=180,
))

# 2. Komut: [hedef yön (derece), hedef irtifa (metre), gaz].
blue_command = [0, 5000, 0.99]
red_command = [180, 7000, 0.99]

# İstersen hedef seçip ateşle; otomatik atış yok.
# blue.set_target(red)
# blue.launch_missile()

# 3. Komutları uygula. Yeni komutlarını bu döngüde verebilirsin.
while blue.simObj.get_sim_time_sec() < 180:
    blue.apply_action(blue_command)
    red.apply_action(red_command)

    # Veri okuma örneği: altitude_m = blue.simObj.get_altitude()
    time.sleep(0.08)  # Yaklaşık gerçek zaman; kaldırırsan beklemeden çalışır.
