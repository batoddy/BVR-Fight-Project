"""BVRGym's own environment and Tacview logger; no viewer dependencies."""
import os
from pathlib import Path
import sys

parent = Path(__file__).resolve().parents[2]
workspace = parent if (parent / 'BVRGym').is_dir() else parent / 'Workspace'
bvrgym = workspace / 'BVRGym'
sys.path.insert(0, str(bvrgym))
os.chdir(bvrgym)

from jsb_gym.envs.BaseEnv import BVRBase
from jsb_gym.envs.config import baseEnv_conf

baseEnv_conf.tacview_output_dir = str(workspace / 'recordings/example')
baseEnv_conf.max_episode_time = 180
env = BVRBase(baseEnv_conf)
env.reset()
env.log_tacview()  # Initialize BVRGym's native logger.
env.tacview_logger.log_flight_data()

try:
    done = False
    while not done:
        _, _, done, truncated, _ = env.step(env.action_space.sample())
        env.log_tacview()
        done = done or truncated
except KeyboardInterrupt:
    print('\nSimülasyon durduruldu.')
finally:
    env.tacview_logger.save_logs()
    print('Tacview CSV kaydı:', baseEnv_conf.tacview_output_dir)
