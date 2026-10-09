"""Generate real native CSVs and verify recording does not alter flight dynamics."""
import argparse
import copy
import csv
import hashlib
import json
import os
from pathlib import Path
import sys
from types import ModuleType, SimpleNamespace

parser = argparse.ArgumentParser()
parser.add_argument('--bvrgym', required=True, type=Path)
parser.add_argument('--output', required=True, type=Path)
args = parser.parse_args()
repository, output = args.bvrgym.resolve(), args.output.resolve()
sys.path.insert(0, str(repository))
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
os.environ['JSBSIM_DEBUG'] = '0'
os.chdir(repository)

from jsb_gym.agents.agents import BaseBVRAgent
from jsb_gym.agents.config import blue_agent, red_agent
from jsb_gym.utils.loggers import TacviewLogger
from recordings import read_recording

sources = [repository / name for name in ('jsb_gym/utils/loggers.py', 'jsb_gym/envs/BaseEnv.py', 'jsb_gym/agents/agents.py')]
hashes = {str(path): hashlib.sha256(path.read_bytes()).hexdigest() for path in sources}


def clone(module):
    return SimpleNamespace(**{key: copy.deepcopy(value) for key, value in vars(module).items()
                              if not key.startswith('_') and not isinstance(value, ModuleType)})


def create(template, lat, heading, name):
    aircraft, missile = clone(template.aircraft_simObj_conf), clone(template.missile_simObj_conf)
    aircraft.data_output_xml = missile.data_output_xml = None
    aircraft.fg_sleep_time = missile.fg_sleep_time = None
    aircraft.aircraft_simulation.Sim_time_step = missile.missile_simulation.Sim_time_step = 1
    parameters = copy.deepcopy(template.agent_parameters)
    parameters.lat, parameters.long, parameters.alt, parameters.vel, parameters.heading = lat, 18, 7000, 300, heading
    parameters.ammo = 2
    conf = SimpleNamespace(agent_parameters=parameters, aircraft_simObj_conf=aircraft,
                           missile_simObj_conf=missile, agent_name=name,
                           aircraft_name=template.aircraft_name, team=template.team)
    return BaseBVRAgent(conf, None)


def fly(record=False):
    blue, red = create(blue_agent, 59, 0, 'Blue'), create(red_agent, 59.25, 180, 'Red')
    blue.set_target(red)
    red.set_target(blue)
    logger = TacviewLogger(SimpleNamespace(conf=SimpleNamespace(tacview_output_dir=str(output)), all_agents=[blue, red])) if record else None
    if logger:
        logger.log_flight_data()
    expected = []
    launch_time = None
    for index in range(16):
        if index == 5:
            launch_time = blue.simObj.get_sim_time_sec()
            blue.launch_missile()
            red.launch_missile()
        if index == 8:
            blue.ammo['1'].set_target(red)
            blue.ammo['1'].launch(blue)
        blue.apply_action([0, 7000, .49])
        red.apply_action([180, 7000, .49])
        if logger:
            logger.log_flight_data()
            expected.append((blue.simObj.get_sim_time_sec(), blue.simObj.get_psi(), blue.simObj.get_altitude()))
    result = [(agent.simObj.get_sim_time_sec(), agent.simObj.get_lat_gc_deg(), agent.simObj.get_long_gc_deg(),
               agent.simObj.get_altitude(), agent.simObj.get_psi(), agent.healthPoints) for agent in (blue, red)]
    if logger:
        logger.save_logs()
    return result, expected, launch_time


baseline, _, _ = fly()
actual, expected, launch_time = fly(record=True)
assert baseline == actual, (baseline, actual)
samples, objects = read_recording(output)
assert len(objects) == 5, objects
blue = [row for row in samples if row['id'] == 'F-16 (Blue) [Blue].csv']
assert len(blue) == 17
assert [(row['sim_time_s'], row['heading_deg'], row['altitude_m']) for row in blue[1:]] == expected
missiles = [row for row in objects if row['kind'] == 'missile']
assert len(missiles) == 3
assert all(row['record_start_s'] > launch_time for row in missiles)
assert {row['team'] for row in missiles} == {'blue', 'red'}
for path in sources:
    assert hashlib.sha256(path.read_bytes()).hexdigest() == hashes[str(path)]
for path in output.glob('*.csv'):
    if path.stat().st_size > 4:
        with path.open(newline='') as handle:
            assert next(csv.reader(handle)) == ['Time','Longitude','Latitude','Altitude','Roll (deg)','Pitch (deg)','Yaw (deg)']
print(json.dumps(dict(passed=True, nativeLogger=True, physicsUnchanged=True, sourceUnchanged=True,
                      objects=len(objects), missiles=len(missiles), samples=len(samples), directory=str(output)), ensure_ascii=False))
