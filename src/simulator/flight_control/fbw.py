"""JSBSim F-16 FBW adaptörü: yatış hızı ve yük komutunu kumanda girdisine çevirir (TASARIM.md katman 6).

F-16 FBW yatışta hız komutu ister (aileron-cmd-norm 1.0 ≈ 180°/s). Yunuslamada g ve q karışımı ister;
temiz g komutu olmadığı için ölçülen yük ile komut farkından PI kullanılır.
"""
import numpy as np

from simulator.flight_control import parameters as P


class FBWAdapter:
    def __init__(self):
        self.g_integral = 0.0                            # elevator PI integrali; çubuk nötrden başlar
        self.saturated_s = 0.0                           # s, elevator'ın kesintisiz tam çekişte kaldığı süre

    def update(self, model, roll_rate_cmd, load_cmd, throttle, dt_s):
        """roll_rate_cmd: rad/s (+ sağa), load_cmd: g, throttle: 0..1."""
        aileron = float(np.clip(roll_rate_cmd * P.AILERON_PER_RAD_S, -1.0, 1.0))
        error = load_cmd - model.load_factor
        self.g_integral = float(np.clip(self.g_integral + P.KI_G * error * dt_s, -1.0, 1.0))   # kırpma: doygunlukta birikmesin
        elevator = float(np.clip(-(P.KP_G * error + self.g_integral), -1.0, 1.0))           # negatif = çek
        self.saturated_s = self.saturated_s + dt_s if elevator <= -1.0 else 0.0
        model.set_controls(aileron=aileron, elevator=elevator, rudder=0.0, throttle=throttle)  # rudder: FBW yaw kanalı yanal yükü sıfırlar

    def load_unreachable(self):
        """FBW istenen yüke ulaşamıyor mu: elevator SATURATED_S'den uzun tam çekişte."""
        return self.saturated_s > P.SATURATED_S
