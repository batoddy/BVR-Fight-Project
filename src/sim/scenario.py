"""Senaryo dosyasını oku; proje yollarını ve temel ayar kontrollerini bir arada tut."""

import math
from pathlib import Path

import yaml


# =============================================================================
# DOSYA YOLLARI
# Terminalin konumu yerine bu dosyanın konumunu esas alırız.
# =============================================================================

SIM_DIRECTORY = Path(__file__).resolve().parent
WORKSPACE = SIM_DIRECTORY.parents[1]
SCENARIO_PATH = SIM_DIRECTORY / "scenario.yaml"


# =============================================================================
# SENARYOYU OKUMA VE DOĞRULAMA
# =============================================================================

def load_scenario():
    """scenario.yaml içeriğini sözlük olarak oku; henüz simülasyon oluşturma."""
    with SCENARIO_PATH.open(encoding="utf-8-sig") as file:
        return yaml.safe_load(file)


def validate_scenario(scenario):
    """Dosyadan veya doğrudan sözlükten gelen senaryoya aynı kontrolleri uygula."""
    world = scenario["world"]
    step = world["sim_time_step"]
    if type(step) is not int or step < 1:
        raise ValueError("world.sim_time_step pozitif tam sayı olmalı.")

    for key in ("duration_s", "sleep_s"):
        value = world[key]
        if type(value) not in (int, float) or not math.isfinite(value) or value < 0:
            raise ValueError(f"world.{key} sonlu ve negatif olmayan sayı olmalı.")

    if not scenario["agents"]:
        raise ValueError("Senaryoda en az bir ajan bulunmalı.")
