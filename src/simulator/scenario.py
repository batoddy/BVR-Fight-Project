"""Senaryo dosyasını oku ve doğrula; proje yollarını tut."""
import math
from pathlib import Path

import yaml

SIM_DIRECTORY = Path(__file__).resolve().parent
WORKSPACE = SIM_DIRECTORY.parents[1]
SCENARIO_PATH = SIM_DIRECTORY / "scenario.yaml"


def load_scenario(path=SCENARIO_PATH):
    with Path(path).open(encoding="utf-8-sig") as file:
        return yaml.safe_load(file)


def validate_scenario(scenario):
    world = scenario["world"]
    steps = world["physics_steps_per_tick"]
    if type(steps) is not int or steps < 1:
        raise ValueError("world.physics_steps_per_tick pozitif tam sayı olmalı.")
    for key in ("duration_s", "sleep_s"):
        value = world[key]
        if type(value) not in (int, float) or not math.isfinite(value) or value < 0:
            raise ValueError(f"world.{key} sonlu ve negatif olmayan sayı olmalı.")
    if not scenario["agents"]:
        raise ValueError("Senaryoda en az bir ajan bulunmalı.")
