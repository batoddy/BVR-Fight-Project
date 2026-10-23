"""Görüntüleyiciyi elle denemek için gerçek bir örnek kayıt üret.

Normal uçuş yolu (`Workspace/src/bvrgym_sim.py`) otomatik atış yapmaz ve yapmamalı.
Bu script yalnızca arayüzü denemek içindir: önceden belirlenmiş saniyelerde hedef
atar, ateşler ve komutu değiştirir. Böylece kayıtta her olay türü bulunur —
hedef ataması, fırlatma, isabet, kayıp, sağlık değişimi ve hedefin kaldırılması.

Fizik ve güdüm BVRGym'in kendisidir; burada hiçbir ölçüm üretilmez veya değiştirilmez.
Sonuçlar (isabet mi, kayıp mı) senaryonun geometrisinden çıkar, dayatılmaz.

Kullanım, Workspace klasöründen:
    python BVRGym-Goruntuleyici/tests/ornek_kayit.py
    python BVRGym-Goruntuleyici/tests/ornek_kayit.py --output recordings --duration 100
"""
import argparse
import copy
import os
import sys
from pathlib import Path
from types import ModuleType, SimpleNamespace

VIEWER = Path(__file__).resolve().parents[1]
WORKSPACE = VIEWER.parent

# Senaryo burada tanımlıdır: örnek kayıt kullanıcının scenario.yaml'ını değiştirse
# tekrarlanabilir olmaz. Aynı sözlük manifest'e de yazılır.
SCENARIO = {
    "world": {"duration_s": 100, "sleep_s": 0.0, "sim_time_step": 1},
    "agents": {
        "blue": {"name": "KE", "team": "Blue", "ammo": 2, "throttle": 0.49,
                 "spawn": {"lat": 59.0, "long": 18.0, "alt": 8000, "vel": 300, "heading": 0}},
        "red": {"name": "Hostile", "team": "Red", "ammo": 2, "throttle": 0.49,
                "spawn": {"lat": 59.3, "long": 18.0, "alt": 8000, "vel": 300, "heading": 180}},
    },
}

# (simülasyon saniyesi, ne yapılacağı). Uzun menzilden atılan füze enerjisini
# yitirip kaybedilir; yakından atılan isabet eder. İkisi de gerçek sonuçtur.
SCRIPT = (
    (2.0, "blue_fires"),
    (20.0, "blue_turns"),
    (35.0, "red_fires"),
    (75.0, "blue_clears_target"),
)


def clone(module):
    return SimpleNamespace(**{key: copy.deepcopy(value) for key, value in vars(module).items()
                              if not key.startswith("_") and not isinstance(value, ModuleType)})


def main():
    parser = argparse.ArgumentParser(description="Arayüz denemesi için örnek kayıt üret")
    parser.add_argument("--output", type=Path, default=WORKSPACE / "recordings",
                        help="Kayıt klasörlerinin oluşacağı dizin")
    parser.add_argument("--duration", type=float, default=SCENARIO["world"]["duration_s"],
                        help="Simülasyon süresi, saniye")
    options = parser.parse_args()
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8")

    scenario = copy.deepcopy(SCENARIO)
    scenario["world"]["duration_s"] = options.duration
    sys.path.insert(0, str(WORKSPACE / "src"))
    from sim.recorder import Recorder

    sys.path.insert(0, str(WORKSPACE / "BVRGym"))
    os.chdir(WORKSPACE / "BVRGym")  # BVRGym'in göreli XML/model yolları için.
    from data_classes.agent_dataclass import Agent_parameters
    from jsb_gym.agents.agents import BaseBVRAgent
    from jsb_gym.simObjects.config import f16_config, AAM_config

    def create(settings):
        aircraft, missile = clone(f16_config), clone(AAM_config)
        aircraft.aircraft_simulation.Sim_time_step = scenario["world"]["sim_time_step"]
        missile.missile_simulation.Sim_time_step = scenario["world"]["sim_time_step"]
        return BaseBVRAgent(SimpleNamespace(
            agent_name=settings["name"], team=settings["team"], aircraft_name="F-16",
            agent_parameters=Agent_parameters(ammo=settings["ammo"], **settings["spawn"]),
            aircraft_simObj_conf=aircraft, missile_simObj_conf=missile), None)

    agents = {key: create(settings) for key, settings in scenario["agents"].items()}
    commands = {key: [settings["spawn"]["heading"], settings["spawn"]["alt"], settings["throttle"]]
                for key, settings in scenario["agents"].items()}
    blue, red = agents["blue"], agents["red"]

    def act(name):
        if name == "blue_fires":
            blue.set_target(red)
            blue.launch_missile()
        elif name == "blue_turns":
            commands["blue"] = [350, 8000, 0.69]
        elif name == "red_fires":
            red.set_target(blue)
            red.launch_missile()
        elif name == "blue_clears_target":
            blue.set_target(None)

    clock = blue.simObj
    start = clock.get_sim_time_sec()
    pending = list(SCRIPT)
    with Recorder(options.output, scenario=scenario) as recorder:
        recorder.record(agents, commands)
        while clock.get_sim_time_sec() - start < scenario["world"]["duration_s"]:
            elapsed = clock.get_sim_time_sec() - start
            while pending and pending[0][0] <= elapsed:
                act(pending.pop(0)[1])
            for key, agent in agents.items():
                agent.apply_action(commands[key])
            recorder.record(agents, commands)
        directory, snapshots, events = recorder.directory, recorder.snapshots, recorder.event_count
    print(f"Örnek kayıt: {directory}")
    print(f"{snapshots} snapshot · {events} olay · sağlık: "
          + " · ".join(f"{agent.conf.agent_name} {agent.healthPoints:g}" for agent in agents.values()))
    print("Arayüzde aç: python BVRGym-Goruntuleyici/viewer.py")


if __name__ == "__main__":
    main()
