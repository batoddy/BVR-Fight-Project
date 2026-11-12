"""Uçuş kontrolü: geometri, manevra düzlemi işaretleri, yatış hızı komutu ve kısa bir dönüş.

Çalıştırma (Workspace): python -m unittest discover -s src/simulator/tests -t src
"""
import math
import sys
import unittest
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))   # src/

from simulator.flight_control import parameters as P
from simulator.flight_control.aml import (desired_direction, desired_gamma, lift_command, roll_rate_command)
from simulator.flight_control.geometry import bank_angle, body_axes, direction, wrap_pi
from simulator.simulation import Simulation

NORTH = np.array([1.0, 0.0, 0.0])
EAST = np.array([0.0, 1.0, 0.0])
SPEED = 270.0                                    # m/s, ~M0.9 10 km


def bank_deg(lift, v_hat=NORTH):
    return math.degrees(bank_angle(lift, v_hat, EAST))


class GeometryTest(unittest.TestCase):
    def test_wrap(self):
        self.assertAlmostEqual(wrap_pi(math.radians(190)), math.radians(-170))
        self.assertAlmostEqual(wrap_pi(math.radians(-190)), math.radians(170))

    def test_direction(self):
        np.testing.assert_allclose(direction(math.radians(90), 0.0), EAST, atol=1e-12)
        self.assertAlmostEqual(direction(0.0, math.radians(30))[2], -0.5)          # tırmanış: aşağı bileşen negatif

    def test_body_axes_level_north(self):
        x, y, z = body_axes(0.0, 0.0, 0.0)
        np.testing.assert_allclose(np.vstack((x, y, z)), np.eye(3), atol=1e-12)

    def test_body_axes_orthonormal(self):
        axes = np.vstack(body_axes(0.4, -0.3, 2.0))
        np.testing.assert_allclose(axes @ axes.T, np.eye(3), atol=1e-12)

    def test_bank_angle_matches_roll(self):
        for roll in (-120.0, -30.0, 0.0, 45.0, 170.0):
            _, _, z = body_axes(math.radians(roll), 0.0, 0.0)
            self.assertAlmostEqual(bank_deg(-z), roll, places=9)                   # kaldırma −z gövde

    def test_bank_angle_signs(self):
        self.assertAlmostEqual(bank_deg(np.array([0.0, 0.0, -1.0])), 0.0)          # yukarı: düz
        self.assertAlmostEqual(bank_deg(EAST), 90.0)                               # sağa
        self.assertAlmostEqual(bank_deg(-EAST), -90.0)                             # sola


class DesiredDirectionTest(unittest.TestCase):
    def test_lead_clipped(self):
        target = desired_direction(math.radians(180), 0.0, 0.0)                    # 180°: en fazla 90° önde
        self.assertAlmostEqual(abs(target[1]), 1.0, places=9)
        target = desired_direction(math.radians(30), 0.0, 0.0)
        np.testing.assert_allclose(target, direction(math.radians(30), 0.0), atol=1e-12)
        target = desired_direction(math.radians(350), 0.0, 0.0)                    # 350°: sola 10°
        self.assertLess(target[1], 0.0)

    def test_gamma_limits(self):
        self.assertAlmostEqual(desired_gamma(5000.0, SPEED), math.atan2(P.VZ_UP_MAX, SPEED))
        self.assertAlmostEqual(desired_gamma(-5000.0, SPEED), math.atan2(-P.VZ_DOWN_MAX, SPEED))
        self.assertAlmostEqual(desired_gamma(100.0, SPEED), math.atan2(P.K_H * 100.0, SPEED))


class LiftCommandTest(unittest.TestCase):
    def test_level_flight_wings_level_1g(self):
        lift, plane = lift_command(NORTH, SPEED, NORTH, 6.0, EAST)
        self.assertFalse(plane)
        self.assertAlmostEqual(bank_deg(lift), 0.0)
        self.assertAlmostEqual(np.linalg.norm(lift) / P.G0_MPS2, 1.0)

    def test_large_right_turn_maneuver_plane(self):
        lift, plane = lift_command(NORTH, SPEED, EAST, 6.0, EAST)
        self.assertTrue(plane)
        self.assertAlmostEqual(np.linalg.norm(lift) / P.G0_MPS2, 6.0)
        self.assertAlmostEqual(bank_deg(lift), math.degrees(math.acos(1.0 / 6.0)), places=6)   # GRAVITY_COMPENSATION 1: n·cosφ = 1

    def test_large_left_turn_negative_bank(self):
        lift, plane = lift_command(NORTH, SPEED, -EAST, 3.0, EAST)
        self.assertTrue(plane)
        self.assertAlmostEqual(bank_deg(lift), -math.degrees(math.acos(1.0 / 3.0)), places=6)

    def test_small_correction_pointing(self):
        lift, plane = lift_command(NORTH, SPEED, direction(math.radians(2), 0.0), 6.0, EAST)
        self.assertFalse(plane)
        self.assertGreater(bank_deg(lift), 0.0)
        expected = math.degrees(math.atan2(SPEED * P.K_POINT * math.radians(2), P.G0_MPS2))
        self.assertAlmostEqual(bank_deg(lift), expected, places=1)

    def test_descent_never_inverted(self):
        lift, _ = lift_command(NORTH, SPEED, direction(0.0, math.radians(-40)), 6.0, EAST)
        self.assertAlmostEqual(bank_deg(lift), 0.0)
        self.assertAlmostEqual(np.linalg.norm(lift), P.G0_MPS2 - P.A_DOWN_MAX)


class RollRateCommandTest(unittest.TestCase):
    QBAR = 15000.0                                # Pa, 10 km M0.9

    def test_linear_region(self):
        error = math.radians(2.0)
        self.assertAlmostEqual(roll_rate_command(error, self.QBAR), P.K_PHI * error)
        self.assertAlmostEqual(roll_rate_command(-error, self.QBAR), -P.K_PHI * error)

    def test_rate_limit(self):
        self.assertAlmostEqual(roll_rate_command(math.radians(170), 1e6), math.radians(P.P_MAX_DPS))

    def test_bang_bang_region(self):
        error = math.radians(90.0)
        pdot = math.radians(P.PDOT_PER_QBAR * 2000.0 / 1000.0)                    # düşük q̄: ivme sınırı baskın
        self.assertAlmostEqual(roll_rate_command(error, 2000.0), math.sqrt(2.0 * pdot * error))


class TurnScenarioTest(unittest.TestCase):
    def test_turn_30_deg(self):
        agent = dict(name="T", type="F16", team="Blue", ammo=0, throttle=0.6,
                     spawn=dict(lat=59.0, long=18.0, alt=8000.0, vel=260.0, heading=0.0))
        simulation = Simulation(dict(world=dict(duration_s=30.0, sleep_s=0.0, physics_steps_per_tick=10),
                                     agents=dict(blue=agent)))
        model = simulation.aircraft["blue"].model
        command = [dict(id="blue", heading_deg=30.0, altitude_m=8000.0, throttle=0.6, g_limit=4.0)]
        for _ in range(12 * 20):                                                  # 20 s
            simulation.step(command)
        _, track = model.flight_path_deg
        self.assertLess(abs(track - 30.0), 1.0)
        self.assertLess(abs(model.alt_m - 8000.0), 50.0)
        self.assertLess(abs(model.roll_deg), 5.0)


if __name__ == "__main__":
    unittest.main()
