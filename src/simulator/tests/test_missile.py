"""Füze altyapısı: atmosfer, Dünya dönüşümleri, sürükleme eğrisi, atış ve uçuş yaşam döngüsü.

Güdüm/arayıcı/tapa boşken füze güdümsüz uçar; bu testler o haliyle geçer.
"""
import math
import sys
import unittest
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))   # src/

from simulator.data import load_missile
from simulator.physics import atmosphere, earth
from simulator.scenario import load_scenario
from simulator.simulation import Simulation
from simulator.weapons.missile import turn_bleed, zero_lift_drag_coefficient


class AtmosphereTest(unittest.TestCase):
    def test_sea_level(self):                                  # USSA 1976 Tablo 1
        temperature, pressure, density, sound = atmosphere.state(0.0)
        self.assertAlmostEqual(temperature, 288.15, places=2)
        self.assertAlmostEqual(pressure, 101325.0, delta=1.0)
        self.assertAlmostEqual(density, 1.2250, delta=1e-4)
        self.assertAlmostEqual(sound, 340.294, delta=0.01)

    def test_tropopause(self):                                 # 11 km jeopotansiyel = 11019 m geometrik
        z = atmosphere.R0_M * 11000.0 / (atmosphere.R0_M - 11000.0)
        temperature, pressure, density, _ = atmosphere.state(z)
        self.assertAlmostEqual(temperature, 216.65, places=2)
        self.assertAlmostEqual(pressure, 22632.06, delta=1.0)
        self.assertAlmostEqual(density, 0.36392, delta=1e-4)


class EarthTest(unittest.TestCase):
    def test_round_trip(self):
        for lat, lon, alt in ((59.0, 18.0, 10000.0), (-33.9, 151.2, 0.0), (0.0, 0.0, 25000.0)):
            back = earth.ecef_to_geodetic(earth.geodetic_to_ecef(lat, lon, alt))
            self.assertAlmostEqual(back[0], lat, places=8)
            self.assertAlmostEqual(back[1], lon, places=8)
            self.assertAlmostEqual(back[2], alt, delta=1e-3)

    def test_ned_axes_orthonormal(self):
        axes = earth.ned_axes(59.0, 18.0)
        self.assertTrue(np.allclose(axes @ axes.T, np.eye(3)))
        down = earth.ned_to_ecef([0, 0, 1], 59.0, 18.0)
        up_from_center = earth.geodetic_to_ecef(59.0, 18.0, 1000.0) - earth.geodetic_to_ecef(59.0, 18.0, 0.0)
        self.assertAlmostEqual(float(np.dot(down, up_from_center / 1000.0)), -1.0, places=9)


class DragTest(unittest.TestCase):
    def test_curve_continuous(self):                           # OPRF parçalı eğrisi sınırlarda sürekli
        for edge in (0.7, 1.2):
            below = zero_lift_drag_coefficient(edge - 1e-9, 0.3)
            above = zero_lift_drag_coefficient(edge + 1e-9, 0.3)
            self.assertAlmostEqual(below, above, delta=0.01)

    def test_bleed(self):
        self.assertEqual(turn_bleed(0.0, 5000.0), 0.0)
        self.assertGreater(turn_bleed(20.0, 0.0), turn_bleed(20.0, 10000.0))   # alçakta daha çok kayıp (OPRF tablosu)


class MissileLifecycleTest(unittest.TestCase):
    def setUp(self):
        self.simulation = Simulation(load_scenario())
        self.commands = self.simulation.read_flight_commands()

    def missile_states(self):
        return {m["id"]: m for m in self.simulation.read_state()["missiles"]}

    def test_fire(self):
        blue = self.simulation.aircraft["blue"]
        ammo = blue.ammo
        self.assertTrue(self.simulation.fire("blue", "red"))
        self.assertEqual(blue.ammo, ammo - 1)
        self.assertFalse(self.simulation.fire("blue", "blue"))
        state = self.missile_states()["blue/missile/0"]
        self.assertEqual((state["state"], state["target_id"]), ("in_flight", "red"))
        self.assertIsNotNone(state["kinematics"])

    def test_burn_and_end(self):
        profile = load_missile()
        self.simulation.fire("blue", "red")
        missile = self.simulation.aircraft["blue"].missiles[0]
        start_speed = np.linalg.norm(missile.velocity)
        while missile.time_s < profile.burn_s + 0.1:
            self.simulation.step(self.commands)
        self.assertGreater(np.linalg.norm(missile.velocity), start_speed + 300.0)   # yanmada hızlanır
        self.assertAlmostEqual(missile.mass_kg, profile.mass_launch_kg - profile.propellant_kg, delta=0.5)
        while missile.state == "in_flight" and not self.simulation.finished():
            self.simulation.step(self.commands)
        self.assertEqual((missile.state, missile.outcome), ("ended", "lost"))     # güdümsüz: isabet yok
        self.assertEqual(self.missile_states()["blue/missile/0"]["outcome"], "lost")


if __name__ == "__main__":
    unittest.main()
