import os
import sys
import time
import copy
from pathlib import Path
from types import ModuleType, SimpleNamespace

workspace = Path(__file__).resolve().parents[1]
bvrgym = workspace / "BVRGym"

sys.path.insert(0, str(bvrgym))
os.chdir(bvrgym)

from jsb_gym.agents.agents import RLBVRAgent
from jsb_gym.agents.config import blue_agent, red_agent
from jsb_gym.utils.loggers import TacviewLogger


# Her agent için bağımsız konfigürasyon oluştur.
# Böylece uçuş ayarları ajanlar arasında paylaşılmaz.
def copy_config(config):
    return SimpleNamespace(**{
        key: copy.deepcopy(value)
        for key, value in vars(config).items()
        if not key.startswith("_")
        and not isinstance(value, ModuleType)
    })


def create_agent(template, lat, heading):
    aircraft_config = copy_config(template.aircraft_simObj_conf)
    missile_config = copy_config(template.missile_simObj_conf)

    aircraft_config.data_output_xml = None
    missile_config.data_output_xml = None

    aircraft_config.fg_sleep_time = None
    missile_config.fg_sleep_time = None
    aircraft_config.aircraft_simulation.Sim_time_step = 1
    missile_config.missile_simulation.Sim_time_step = 1

    parameters = copy.deepcopy(template.agent_parameters)
    parameters.lat = lat
    parameters.long = 18.0
    parameters.alt = 7000.0
    parameters.vel = 300.0
    parameters.heading = heading
    parameters.ammo = 2

    config = SimpleNamespace(
        agent_parameters=parameters,
        aircraft_simObj_conf=aircraft_config,
        missile_simObj_conf=missile_config,
        aircraft_name=template.aircraft_name,
        agent_name=template.agent_name,
        team=template.team,
    )

    return RLBVRAgent(config, None)


# Havada başlangıç: iki uçak birbirine doğru uçuyor.
blue = create_agent(blue_agent, 59.0, 0)
red = create_agent(red_agent, 59.25, 180)

blue.set_target(red)
red.set_target(blue)

# BVRGym'in kendi logger'ı yalnızca all_agents ve çıktı klasörünü ister.
# Aynı CSV dosyaları hem Tacview'de hem bağımsız görüntüleyicide açılır.
recording = workspace / "recordings" / "agent_ucus"
scenario = SimpleNamespace(
    conf=SimpleNamespace(tacview_output_dir=str(recording)),
    all_agents=[blue, red],
)
logger = TacviewLogger(scenario)
logger.log_flight_data()

print("İki agent oluşturuldu. Uçuş başladı.")
print("Tacview CSV kayıt klasörü:", recording)

try:
    while blue.simObj.get_sim_time_sec() < 180:
        started = time.perf_counter()
        previous_time = blue.simObj.get_sim_time_sec()

        # Komutlar: heading, altitude, throttle.
        # Agent, atış koşulları sağlanınca kendisi füze ateşler.
        blue.apply_action([0, 7000, 0.49])
        red.apply_action([180, 7000, 0.49])
        logger.log_flight_data()

        if blue.healthPoints <= 0 or red.healthPoints <= 0:
            print(
                "Angajman sona erdi.",
                "Blue:", blue.healthPoints,
                "Red:", red.healthPoints,
            )
            break

        # Yaklaşık gerçek zamanlı çalıştır.
        elapsed_sim = blue.simObj.get_sim_time_sec() - previous_time
        elapsed_real = time.perf_counter() - started
        time.sleep(max(0, elapsed_sim - elapsed_real))

except KeyboardInterrupt:
    print("\nUçuş durduruldu.")
finally:
    logger.save_logs()
    print("Tacview CSV kayıtları kaydedildi:", recording)
