"""Senaryodan uçak oluştur, uçuş komutlarını ver, durumu kaydet.

Bu bir şablondur: kendi karar kodun 4. bölümdeki döngüye girer. Otomatik hedef
seçimi, atış veya manevra yoktur — o kararlar ileride C++ tarafından gelecek.

Çalıştırma (Workspace klasöründen):  python src/bvrgym_sim.py
Ayarlar: scenario.yaml   Kayıt: recordings/<zaman>/

Üç ayrı zaman var, karıştırmak kolay:
  dt = 1/120 s           JSBSim fizik adımı, XML'de sabit.
  Control_time_step = 10 PID hatasının sabit tutulduğu kare sayısı → 83,3 ms.
  sim_time_step          Bir apply_action() kaç tane 83,3 ms çevrimi koşar.
                         Dış komut frekansıdır: 1 → 12 Hz, 6 → 2 Hz (BVRGym varsayılanı).
`sim_time_step` saniye değildir ve fizik çözünürlüğünü değiştirmez.
"""
import copy
import os
import sys
import time
from pathlib import Path
from types import ModuleType, SimpleNamespace

import yaml
from sim.recorder import Recorder

sys.stdout.reconfigure(encoding="utf-8")

SCENARIO_YAML = "scenario.yaml"
BVRGYM_PATH = "BVRGym"
RECORDINGS_PATH = "recordings"

# ============================================================================
# 1. Senaryo ve BVRGym
# ============================================================================
WORKSPACE = Path(__file__).resolve().parents[1]
with (WORKSPACE / SCENARIO_YAML).open(encoding="utf-8-sig") as file:
    scenario = yaml.safe_load(file)
world = scenario["world"]

# BVRGym model XML'lerini kendi klasörüne göreli açtığı için çalışma dizinini
# değiştiriyoruz; paketleri de orada. İkisi de import'lardan önce olmalı.
sys.path.insert(0, str(WORKSPACE / BVRGYM_PATH))
os.chdir(WORKSPACE / BVRGYM_PATH)

from data_classes.agent_dataclass import Agent_parameters
from jsb_gym.agents.agents import BaseBVRAgent
from jsb_gym.simObjects.config import f16_config, AAM_config


# ============================================================================
# 2. Ajanlar
# ============================================================================
def copy_config(module):
    """Config modülünü bağımsız kopyaya çevir.

    İki ajan aynı config nesnesini paylaşırsa PID kontrolcülerinin iç durumu da
    paylaşılır ve birinin manevrası diğerini bozar.
    """
    return SimpleNamespace(**{
        key: copy.deepcopy(value) for key, value in vars(module).items()
        if not key.startswith("_") and not isinstance(value, ModuleType)
    })


def create_agent(settings):
    """Bir uçak + `ammo` adet füze. Her biri ayrı bir JSBSim örneğidir."""
    aircraft = copy_config(f16_config)
    missile = copy_config(AAM_config)
    aircraft.aircraft_simulation.Sim_time_step = world["sim_time_step"]
    missile.missile_simulation.Sim_time_step = world["sim_time_step"]
    return BaseBVRAgent(SimpleNamespace(
        agent_name=settings["name"], team=settings["team"], aircraft_name="F-16",
        agent_parameters=Agent_parameters(ammo=settings["ammo"], **settings["spawn"]),
        aircraft_simObj_conf=aircraft,
        missile_simObj_conf=missile,
    ), None)


# scenario.yaml'daki her ajan tanımı için bir uçak oluştur.
#   key      -> "blue", "red"  (YAML'daki anahtar; kayıt dosya adına da girer)
#   settings -> o ajanın ayarları: name, team, ammo, throttle, spawn
agents = {}
for key, settings in scenario["agents"].items():
    agents[key] = create_agent(settings)

# Kısayol: aşağıda agents["blue"] yazmak yerine blue yazabilmek için.
blue = agents["blue"]
red = agents["red"]

# ============================================================================
# 3. Başlangıç komutları
# ============================================================================
# Komut = [hedef yön (derece), hedef irtifa (m), gaz (0-1)]. Hedef değerlerdir;
# PID autopilot uçağı oraya götürmeye çalışır. Hedef hız komutu yoktur.
# Komut kırpılmaz: config'teki aircraft_limits yalnızca BVRGym'in RL
# environment'ında kullanılır, apply_action() ona bakmaz.
#
# Başlangıçta her uçak spawn yönünü ve irtifasını korur, yani düz uçar.
commands = {}
for key, settings in scenario["agents"].items():
    spawn = settings["spawn"]
    commands[key] = [spawn["heading"], spawn["alt"], settings["throttle"]]

# Atış otomatik değil; istersen sen emredersin:
#   blue.set_target(red)
#   blue.launch_missile()   # havada kendi füzesi varken atmaz
# Hedef ve atış olaylarını recorder events.jsonl'a kendisi yazar.

# ============================================================================
# 4. Uçuş döngüsü
# ============================================================================
clock = blue.simObj  # Bütün ajanlar aynı kadar ilerlediği için saatleri birlikte yürür.
start_time = clock.get_sim_time_sec()

with Recorder(WORKSPACE / RECORDINGS_PATH, scenario=scenario) as recorder:
    print("Kayıt:", recorder.directory)
    recorder.record(agents, commands)  # Başlangıç durumu da bir snapshot.

    while clock.get_sim_time_sec() - start_time < world["duration_s"]:
        # KARAR YERİ: durumu oku, `commands` sözlüğünü güncelle, gerekirse ateşle.
        #   blue.simObj.get_altitude()          # m, MSL
        #   blue.simObj.get_psi()               # derece, burun yönü
        #   blue.simObj.get_true_airspeed()     # m/s
        #   blue.simObj.fdm["velocities/mach"]  # katalogdaki herhangi bir property
        #   blue.healthPoints                   # isabet alınca düşer

        for key, agent in agents.items():
            agent.apply_action(commands[key])  # Uçağı ve aktif füzelerini ilerletir.

        recorder.record(agents, commands)

        # Yalnızca gerçek zamana yaklaştırmak için; simülasyon saatini etkilemez.
        if world["sleep_s"] > 0:
            time.sleep(world["sleep_s"])

print(f"Bitti. {recorder.snapshots} snapshot, {recorder.event_count} olay.")

