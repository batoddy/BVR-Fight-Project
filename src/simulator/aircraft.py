"""Bir uçak: kimlik, JSBSim modeli, uçuş kontrolü, son uçuş komutu, füze slotları, radar ve iz dosyası."""
import math

from simulator.data import load_aircraft, load_missile
from simulator.flight_control import FlightCommand, NeutralControl
from simulator.physics import earth
from simulator.physics.jsbsim_model import JSBSimModel
from simulator.sensors.radar import Radar
from simulator.sensors.tracker import Tracker
from simulator.weapons.missile import Missile

AIRCRAFT_DATA = "f16"     # data/aircraft/<ad>.yaml; fizik modeli şimdilik her tipte F-16

def build_missiles(ammo):
    """scenario.yaml ammo: {profil: adet} → slot sırasıyla füzeler (slot 0'dan başlar)."""
    if not isinstance(ammo, dict):
        raise ValueError("ammo, {füze profili: adet} biçiminde olmalı (ör. aim120c5: 6).")
    missiles = []
    for name, count in ammo.items():
        if type(count) is not int or count < 0:
            raise ValueError(f"ammo.{name}: adet negatif olmayan tam sayı olmalı.")
        profile = load_missile(name)
        missiles += [Missile(profile, len(missiles) + i) for i in range(count)]
    return missiles


class Aircraft:
    def __init__(self, key, settings, controller_factory=NeutralControl):
        self.key = key                              # senaryodaki benzersiz anahtar (blue, red)
        self.name = settings["name"]
        self.type = settings["type"]                # C++ hesap profili etiketi; fizik şimdilik F-16
        self.team = settings["team"]
        self.health = 1                             # eski sim: BVRGym healthPoints başlangıcı
        self.missiles = build_missiles(settings["ammo"])
        data = load_aircraft(AIRCRAFT_DATA)
        self.rcs_m2 = data["rcs_m2"]
        self.radar = Radar(data["radar"])
        self.tracker = Tracker()                    # bu uçağın gördükleri (contacts)

        spawn = settings["spawn"]
        self.model = JSBSimModel()
        self.model.reset(spawn["lat"], spawn["long"], spawn["alt"], spawn["vel"], spawn["heading"])
        self.control = controller_factory()
        self.command = FlightCommand(spawn["heading"], spawn["alt"], settings["throttle"])   # ilk hedef: başlangıç durumu

    @property
    def ammo(self):
        """Atılmamış füze sayısı."""
        return sum(missile.state == "ready" for missile in self.missiles)

    def run(self):
        """Bir fizik adımı: önce kumanda, sonra JSBSim."""
        self.control.update(self.command, self.model)
        self.model.run()

    def position_ecef(self):
        return earth.geodetic_to_ecef(self.model.lat_deg, self.model.lon_deg, self.model.alt_m)

    def velocity_ecef(self):
        return earth.ned_to_ecef(self.model.velocity_ned_mps, self.model.lat_deg, self.model.lon_deg)

    @property
    def forward_ecef(self):
        """Burun yönü (gövde x ekseni), ECEF birim vektör. Radar gövdeye bağlı: görüş alanı buna göre (C++ ATA ile aynı eksen)."""
        yaw, pitch = math.radians(self.model.yaw_deg), math.radians(self.model.pitch_deg)
        nose_ned = [math.cos(pitch) * math.cos(yaw), math.cos(pitch) * math.sin(yaw), -math.sin(pitch)]
        return earth.ned_to_ecef(nose_ned, self.model.lat_deg, self.model.lon_deg)
