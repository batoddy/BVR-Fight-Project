import os
import sys
from pathlib import Path
from types import SimpleNamespace

# BVRGym kodlarını ve XML/model dosyalarını bul.
bvrgym_path = Path(__file__).resolve().parent.parent / "BVRGym"

sys.path.insert(0, str(bvrgym_path))
os.chdir(bvrgym_path)

# ===========================================================================

from jsb_gym.simObjects.config import f16_config    
from jsb_gym.simObjects.aircraft import F16BVR
from jsb_gym.utils.loggers import TacviewLogger

workspace = Path(__file__).resolve().parents[1]
f16_config.data_output_xml = None

f16_config.fg_sleep_time = None
f16_config.aircraft_simulation.Sim_time_step = 1

ucak = F16BVR(f16_config)          # Bizim uçak
bogey = F16BVR(f16_config)      # Karşı uçak

print("F-16 oluşturuldu.") 

ucak.reset(
    lat=59.0,       # Enlem, derece 
    long=18.0,      # Boylam, derece
    alt=5000,       # İrtifa, metre
    vel=250,        # Başlangıç hızı, m/s
    heading=90,     # Yön: 90° = doğu
)

bogey.reset(
    lat=59.05,
    long=18.10,
    alt=5000,
    vel=250,
    heading=270,
)

# Ham uçakları BVRGym'in yerleşik kayıt sınıfına tanıt.
recording = workspace / "recordings" / "ucus"
scenario = SimpleNamespace(
    conf=SimpleNamespace(tacview_output_dir=str(recording)),
    all_agents=[
        SimpleNamespace(conf=SimpleNamespace(agent_name="Blue", aircraft_name="F-16", team="Blue"), simObj=ucak, ammo={}),
        SimpleNamespace(conf=SimpleNamespace(agent_name="Red", aircraft_name="F-16", team="Red"), simObj=bogey, ammo={}),
    ],
)
logger = TacviewLogger(scenario)
logger.log_flight_data()
try:
    ucak.step([90, 5000, 0.49])
    bogey.step([270, 5000, 0.49])
    logger.log_flight_data()
finally:
    logger.save_logs()
    print("Tacview CSV kayıtları:", recording)
