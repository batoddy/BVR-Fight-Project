"""İki ajanlı temel şablon. Komut ve ateşleme kararı senaryo kodundan gelir."""
import copy
import os
import sys
import time
from pathlib import Path
from types import ModuleType, SimpleNamespace

WORKSPACE = Path(__file__).resolve().parents[1]
BVRGYM = WORKSPACE / "BVRGym"
sys.path.insert(0, str(BVRGYM))

from data_classes.agent_dataclass import Agent_parameters
from jsb_gym.agents.agents import BaseBVRAgent
from jsb_gym.simObjects.config import f16_config, AAM_config
from jsb_gym.utils.loggers import TacviewLogger

SIM_DURATION = 180
RECORDING_DIR = WORKSPACE / "recordings" / "agent_ucus"


def copy_config(module):
    """Ajanların fizik ve kontrolcü ayarları birbirinden bağımsız olsun."""
    return SimpleNamespace(**{
        name: copy.deepcopy(value)
        for name, value in vars(module).items()
        if not name.startswith("_") and not isinstance(value, ModuleType)
    })


def create_agent(name, team, spawn):
    aircraft, missile = copy_config(f16_config), copy_config(AAM_config)
    aircraft.aircraft_simulation.Sim_time_step = 1
    missile.missile_simulation.Sim_time_step = 1
    for config in (aircraft, missile):
        config.data_output_xml = None
        config.fg_sleep_time = None

    config = SimpleNamespace(
        agent_name=name, team=team, aircraft_name="F-16",
        agent_parameters=spawn,
        aircraft_simObj_conf=aircraft,
        missile_simObj_conf=missile,
    )
    # BVRGym'in göreli XML/model yolları için; sonra Workspace'e geri dön.
    previous_cwd = Path.cwd()
    try:
        os.chdir(BVRGYM)
        return BaseBVRAgent(config, None)
    finally:
        os.chdir(previous_cwd)


def main():
    # 1. Ajanlar: isim, takım, mühimmat ve başlangıç durumu.
    # lat/long/heading: derece, alt: metre, vel: m/s.
    blue = create_agent("Kartal-1", "Blue", Agent_parameters(
        ammo=2, lat=59.0, long=18.0, alt=8000, vel=300, heading=0,
    ))
    red = create_agent("Bogey-1", "Red", Agent_parameters(
        ammo=2, lat=59.25, long=18.0, alt=3000, vel=300, heading=180,
    ))

    # 2. Kullanıcı komutları: [hedef yön (derece), irtifa (metre), gaz].
    blue_command = [0, 5000, 0.99]
    red_command = [180, 7000, 0.99]

    # Atış yalnızca açıkça çağrılır. Bu iki satır örnek olarak kapalıdır:
    # blue.set_target(red)
    # blue.launch_missile()

    # 3. BVRGym'in yerleşik Tacview CSV kaydı.
    logger = TacviewLogger(SimpleNamespace(
        conf=SimpleNamespace(tacview_output_dir=str(RECORDING_DIR)),
        all_agents=[blue, red],
    ))
    logger.log_flight_data()

    # 4. İki ajanı verilen komutlarla ilerlet ve yaklaşık gerçek zamanda çalıştır.
    sim_start = blue.simObj.get_sim_time_sec()
    wall_start = time.monotonic()
    try:
        while blue.simObj.get_sim_time_sec() - sim_start < SIM_DURATION:
            # Dış komut bağlantısı eklendiğinde komutlar burada güncellenecek.
            blue.apply_action(blue_command)
            red.apply_action(red_command)
            logger.log_flight_data()

            elapsed = blue.simObj.get_sim_time_sec() - sim_start
            time.sleep(max(0, wall_start + elapsed - time.monotonic()))
    except KeyboardInterrupt:
        print("Uçuş durduruldu.")
    finally:
        logger.save_logs()
        print("Kayıt:", RECORDING_DIR)


if __name__ == "__main__":
    main()
