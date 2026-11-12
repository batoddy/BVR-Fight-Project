"""Uçuş kontrolü: C++ uçuş komutunu her fizik adımında kumanda girdisine çevirir.

Arayüz: controller.update(command, model) → model.set_controls(...) çağrılır.
Yöntem: NASA AML (plan §3.2), aml.AMLController. NeutralControl: çubuk nötr, karşılaştırma ve deneme için.
"""
from dataclasses import dataclass
from typing import Optional


@dataclass
class FlightCommand:
    """C++'tan gelen uçuş hedefi (TCP mesajındaki alanlar)."""
    heading_deg: float
    altitude_m: float
    throttle: float                      # 0..1; 0.5 üstü art yakıcı (eski sim scenario.yaml notu)
    g_limit: Optional[float] = None      # dönüşte çekilecek en büyük yük; None: kontrolcünün varsayılanı


class NeutralControl:
    """Çubuk nötr, gaz komuttan. Uçuş kontrolü yazılana kadar iskelet testleri için."""

    def update(self, command, model):
        model.set_controls(aileron=0.0, elevator=0.0, rudder=0.0, throttle=command.throttle)
