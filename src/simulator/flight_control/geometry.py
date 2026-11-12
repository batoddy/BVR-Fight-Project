"""NED vektör geometrisi: gövde eksenleri, istenen yön, hız vektörü çevresinde yatış açısı.

NED: x kuzey, y doğu, z aşağı. Açılar radyan (aksi adında belirtilir).
"""
import math

import numpy as np

DOWN = np.array([0.0, 0.0, 1.0])


def wrap_pi(angle):
    """Açıyı −π..π aralığına getir."""
    return (angle + math.pi) % (2.0 * math.pi) - math.pi


def unit(vector):
    norm = np.linalg.norm(vector)
    return vector / norm if norm > 1e-9 else np.zeros(3)


def direction(heading, gamma):
    """Yön (kuzeyden saat yönü) ve uçuş yolu açısından (yukarı +) birim NED vektör."""
    return np.array([math.cos(gamma) * math.cos(heading), math.cos(gamma) * math.sin(heading), -math.sin(gamma)])


def body_axes(roll, pitch, yaw):
    """Euler açılarından gövde x (burun), y (sağ kanat), z (aşağı) eksenleri, NED'de."""
    cr, sr, cp, sp, cy, sy = math.cos(roll), math.sin(roll), math.cos(pitch), math.sin(pitch), math.cos(yaw), math.sin(yaw)
    x = np.array([cp * cy, cp * sy, -sp])
    y = np.array([sr * sp * cy - cr * sy, sr * sp * sy + cr * cy, sr * cp])
    z = np.array([cr * sp * cy + sr * sy, cr * sp * sy - sr * cy, cr * cp])
    return x, y, z


def perpendicular(vector, v_hat):
    """Vektörün v_hat'e dik bileşeni."""
    return vector - np.dot(vector, v_hat) * v_hat


def level_frame(v_hat, fallback_right):
    """Yatışsız rüzgâr ekseni: z = aşağının V̂'ye dik bileşeni, y = z × x (sağ). Dikey uçuşta y yerine fallback."""
    z = unit(perpendicular(DOWN, v_hat))
    if not z.any():                                      # V̂ dikey: aşağı yönü tanımsız
        y = unit(perpendicular(fallback_right, v_hat))
        return np.cross(v_hat, y), y
    return z, np.cross(z, v_hat)


def bank_angle(lift, v_hat, fallback_right):
    """Kaldırma yönünün V̂ çevresindeki açısı; 0: yukarı, +: sağa yatış (gövde yatış açısıyla aynı işaret)."""
    z, y = level_frame(v_hat, fallback_right)
    return math.atan2(np.dot(lift, y), -np.dot(lift, z))
