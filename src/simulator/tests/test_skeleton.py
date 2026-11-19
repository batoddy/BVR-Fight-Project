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

KIN_KEYS = {"lat_deg", "lon_deg", "alt_m", "yaw_deg", "pitch_deg", "roll_deg", "tas_mps", "mach",
            "velocity_ned_mps"}                                     # docs/mimari/MESAJ_PROTOKOLU_V2.md KIN
PACKET_KEYS = {"own", "rwr", "weapons", "contacts", "warnings", "events"}
OWN_KEYS = {"id", "name", "type", "team", "health", "kinematics", "load_factor_g", "fuel_kg", "applied_command"}
WEAPON_KEYS = {"slot", "id", "designation", "state", "request_id", "target_id", "flight_time_s", "kinematics",
               "guidance_mode", "is_locked", "is_pitbull", "outcome"}


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

    def test_packet_schema(self):                                # protokol v2
        state = self.simulation.read_state()
        self.assertEqual(set(state), {"time_s", "aircraft"})
        for key, packet in state["aircraft"].items():
            self.assertEqual(set(packet), PACKET_KEYS)
            self.assertEqual(set(packet["own"]), OWN_KEYS)
            self.assertEqual(set(packet["own"]["kinematics"]), KIN_KEYS)
            self.assertEqual(len(packet["weapons"]), len(self.simulation.aircraft[key].missiles))
            for weapon in packet["weapons"]:
                self.assertEqual(set(weapon), WEAPON_KEYS)
            self.assertEqual([c["id"] for c in packet["contacts"]], [k for k in state["aircraft"] if k != key])   # sensörler kapalı: hep görünür

    def test_hello_lists_slots(self):
        hello = self.simulation.hello()
        blue = next(a for a in hello["aircraft"] if a["id"] == "blue")
        self.assertEqual([w["slot"] for w in blue["weapons"]], list(range(6)))
        self.assertEqual(blue["weapons"][0]["designation"], "AIM-120C")

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
