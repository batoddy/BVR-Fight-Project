"""İskelet: JSBSim modeli, Simulation mesaj şeması, komut doğrulama.

Çalıştırma (Workspace): python -m unittest discover -s src/simulator/tests -t src
"""
import math
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))   # src/

from simulator.physics.jsbsim_model import JSBSimModel
from simulator.scenario import load_scenario
from simulator.simulation import Simulation

AIRCRAFT_KEYS = {"id", "name", "type", "team", "health", "lat_deg", "lon_deg", "alt_msl_m",
                 "yaw_deg", "pitch_deg", "roll_deg", "tas_mps", "mach", "velocity_ned_mps"}   # eski src/sim read_state alanları
MISSILE_KEYS = {"id", "owner_id", "target_id", "state", "outcome", "flight_time_s", "kinematics"}


class ModelTest(unittest.TestCase):
    def test_starts_where_asked(self):
        model = JSBSimModel()
        model.reset(59.0, 18.0, 10000.0, 300.0, 90.0)
        self.assertAlmostEqual(model.lat_deg, 59.0, delta=1e-6)
        self.assertAlmostEqual(model.alt_m, 10000.0, delta=1.0)
        self.assertAlmostEqual(model.yaw_deg, 90.0, delta=0.1)
        self.assertAlmostEqual(model.tas_mps, 300.0, delta=3.0)

    def test_flies_without_nan(self):
        model = JSBSimModel()
        model.reset(59.0, 18.0, 10000.0, 300.0, 0.0)
        model.set_controls(0.0, 0.0, 0.0, 0.45)
        for _ in range(120 * 10):    # 10 s
            model.run()
        self.assertTrue(all(math.isfinite(v) for v in (model.alt_m, model.tas_mps, model.pitch_deg)))
        self.assertGreater(model.alt_m, 5000.0)


class SimulationTest(unittest.TestCase):
    def setUp(self):
        self.simulation = Simulation(load_scenario())

    def test_state_schema_matches_old_sim(self):
        state = self.simulation.read_state()
        self.assertEqual(set(state), {"tick", "sim_time_s", "aircraft", "missiles", "flight_commands"})
        for item in state["aircraft"]:
            self.assertEqual(set(item), AIRCRAFT_KEYS)
        for item in state["missiles"]:
            self.assertEqual(set(item), MISSILE_KEYS)
        self.assertEqual(len(state["missiles"]), sum(p.ammo for p in self.simulation.aircraft.values()))

    def test_step_advances_one_tick(self):
        start = self.simulation.time_s
        self.simulation.step(self.simulation.read_flight_commands())
        self.assertEqual(self.simulation.tick, 1)
        self.assertAlmostEqual(self.simulation.time_s - start, 10 / 120, places=9)

    def test_invalid_command_rejected(self):
        for change in (dict(heading_deg=400.0), dict(throttle=1.5), dict(g_limit=20.0),
                       dict(altitude_m=float("nan"))):
            with self.subTest(change=change):
                commands = self.simulation.read_flight_commands()
                commands[0].update(change)
                with self.assertRaises(ValueError):
                    self.simulation.step(commands)

    def test_missing_aircraft_command_rejected(self):
        with self.assertRaises(ValueError):
            self.simulation.step(self.simulation.read_flight_commands()[:1])


if __name__ == "__main__":
    unittest.main()
