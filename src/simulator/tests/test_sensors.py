"""Radar (menzil, görüş alanı, notch), iz dosyası, RWR ve doğrudan mod."""
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))   # src/

from simulator.scenario import load_scenario
from simulator.simulation import Simulation


def scenario(range_nm, red_heading=180.0, red_alt=10000.0):
    sc = load_scenario()
    sc["world"]["duration_s"] = 60
    sc["world"]["sensors"] = True                     # radar testleri sensörler açıkken
    sc["agents"]["red"]["spawn"].update(lat=59.0 + range_nm / 60.0, heading=red_heading, alt=red_alt)
    return sc


class RadarTest(unittest.TestCase):
    def test_range_formula(self):                     # OPRF apg-68: 70 NM · (2 / 3.2)^(1/4)
        radar = Simulation(scenario(30)).aircraft["blue"].radar
        self.assertAlmostEqual(radar.detection_range_m(2.0) / 1852.0, 70.0 * (2.0 / 3.2) ** 0.25, places=6)

    def test_inside_and_outside_range(self):
        self.assertEqual([c["id"] for c in Simulation(scenario(40)).packet("blue")["contacts"]], ["red"])
        self.assertEqual(Simulation(scenario(70)).packet("blue")["contacts"], [])

    def test_behind_not_seen(self):                   # kırmızı mavinin arkasında (güneyinde)
        sim = Simulation(scenario(-30, red_heading=0.0))
        self.assertEqual(sim.packet("blue")["contacts"], [])

    def test_notch_look_down(self):                   # kırmızı alçakta ve görüş hattına dik (beam) uçuyor
        sim = Simulation(scenario(30, red_heading=90.0, red_alt=5000.0))
        self.assertEqual(sim.packet("blue")["contacts"], [])
        sim = Simulation(scenario(30, red_heading=90.0, red_alt=12000.0))   # yukarı bakış: notch yok
        self.assertEqual([c["id"] for c in sim.packet("blue")["contacts"]], ["red"])

    def test_track_memory_and_rwr(self):
        sim = Simulation(scenario(40))
        self.assertEqual([r["emitter_id"] for r in sim.packet("red")["rwr"]], ["blue"])
        self.assertIn("radar_contact", [e["type"] for e in sim.events["blue"]])
        commands = sim.read_flight_commands()
        commands[0]["heading_deg"] = 180.0             # mavi geri döner: kırmızı arkada kalır
        commands[0]["g_limit"] = 6.0
        for _ in range(12 * 20):
            sim.step(commands)
        contact = sim.packet("blue")["contacts"][0]
        self.assertFalse(contact["tracked"])
        self.assertGreater(contact["age_s"], 0.0)


class SensorsOffTest(unittest.TestCase):
    def test_enemy_always_seen_and_no_rwr(self):        # world.sensors false: radardan önceki davranış
        sc = scenario(90, red_heading=0.0)              # menzil dışında ve arkada: açıkken görülmezdi
        sc["world"]["sensors"] = False
        sim = Simulation(sc)
        contact = sim.packet("blue")["contacts"][0]
        self.assertEqual((contact["id"], contact["tracked"], contact["source"]), ("red", True, "truth"))
        self.assertEqual(sim.packet("red")["rwr"], [])
        self.assertEqual(sim.events["blue"], [])


class DirectModeTest(unittest.TestCase):
    def test_bank_and_g(self):
        sim = Simulation(scenario(70))
        commands = sim.read_flight_commands()
        commands[0] = dict(id="blue", mode="direct", bank_deg=60.0, g=2.0, throttle=0.6)
        for _ in range(12 * 5):
            sim.step(commands)
        model = sim.aircraft["blue"].model
        self.assertAlmostEqual(model.roll_deg, 60.0, delta=3.0)
        self.assertAlmostEqual(model.load_factor, 2.0, delta=0.3)
        self.assertEqual(sim.packet("blue")["own"]["applied_command"]["mode"], "direct")

    def test_invalid_direct_rejected(self):
        sim = Simulation(scenario(70))
        commands = sim.read_flight_commands()
        commands[0] = dict(id="blue", mode="direct", bank_deg=60.0, g=12.0, throttle=0.6)
        with self.assertRaises(ValueError):
            sim.step(commands)


if __name__ == "__main__":
    unittest.main()
