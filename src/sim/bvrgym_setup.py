"""Senaryodaki uçak ve mühimmatları BVRGym/JSBSim nesnelerine dönüştür."""

import copy
import os
import sys
from pathlib import Path
from types import ModuleType, SimpleNamespace

if __package__:
    from . import flight_control
else:
    import flight_control


# =============================================================================
# BVRGYM ORTAMINI HAZIRLAMA
# =============================================================================

def create_agents(scenario, bvrgym_path):
    """Bütün ajanları oluştur ve çağıranın çalışma dizinini geri getir."""
    bvrgym_path = Path(bvrgym_path).resolve()
    if not bvrgym_path.is_dir():
        raise FileNotFoundError(f"BVRGym klasörü bulunamadı: {bvrgym_path}")

    # BVRGym paketleri bu dizinde; XML/model yolları da bu dizine göreli açılır.
    if str(bvrgym_path) not in sys.path:
        sys.path.insert(0, str(bvrgym_path))

    previous_cwd = Path.cwd()
    try:
        os.chdir(bvrgym_path)
        return {
            key: _create_agent(settings, scenario["world"]["sim_time_step"])
            for key, settings in scenario["agents"].items()
        }
    finally:
        # Oluşturma başarısız olsa da terminalin çalışma dizini korunur.
        os.chdir(previous_cwd)


# =============================================================================
# BİR UÇAK VE MÜHİMMATININ KURULUMU
# =============================================================================

def _copy_config(module):
    """Fizik ve PID ayarlarını kopyala; ajanlar birbirinin ayarını değiştirmesin."""
    return SimpleNamespace(**{
        key: copy.deepcopy(value) for key, value in vars(module).items()
        if not key.startswith("_") and not isinstance(value, ModuleType)
    })


def _create_agent(settings, sim_time_step):
    """YAML'daki tek bir ajan tanımından uçak ve füze slotlarını oluştur."""
    # type, C++ hesap profilinin etiketidir; fizik modeli burada hâlâ F-16'dır.
    aircraft_type = settings.get("type", "F16")
    if not isinstance(aircraft_type, str) or not aircraft_type.strip():
        raise ValueError("Ajan type alanı boş olmayan bir metin olmalı.")
    aircraft_type = aircraft_type.strip()

    # Bu import'lar BVRGym yolu hazırlandıktan sonra yapılmalıdır.
    from data_classes.agent_dataclass import Agent_parameters
    from jsb_gym.agents.agents import BaseBVRAgent
    from jsb_gym.simObjects.config import f16_config, AAM_config

    aircraft = _copy_config(f16_config)
    missile = _copy_config(AAM_config)
    aircraft.aircraft_simulation.Sim_time_step = sim_time_step
    missile.missile_simulation.Sim_time_step = sim_time_step

    # Mevcut dalış ayarı: yüksekten gelen füze inişe daha erken başlayabilsin.
    # Önceki denemelerin açıklaması README'deki uçuş ayarları bölümündedir.
    missile.missile_navigation.dive_at = 25e3

    for config in (aircraft, missile):
        config.data_output_xml = None
        config.fg_sleep_time = None  # Gerçek zaman beklemesini ana döngü yönetir.

    agent = BaseBVRAgent(SimpleNamespace(
        agent_name=settings["name"], team=settings["team"], aircraft_name="F-16",
        aircraft_type=aircraft_type,
        agent_parameters=Agent_parameters(ammo=settings["ammo"], **settings["spawn"]),
        aircraft_simObj_conf=aircraft, missile_simObj_conf=missile,
    ), None)

    # Yön/irtifa komutunu uygulayan mevcut kontrolü yalnız bu uçağa bağla.
    flight_control.install(agent.simObj.aircraft_control)
    return agent
