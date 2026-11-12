"""Bir uçak: kimlik, JSBSim modeli, uçuş kontrolü, son uçuş komutu ve füze slotları."""
from simulator.data import load_missile
from simulator.flight_control import FlightCommand, NeutralControl
from simulator.physics import earth
from simulator.physics.jsbsim_model import JSBSimModel
from simulator.weapons.missile import Missile

MISSILE_PROFILE = "aim120c5"     # data/missiles/<ad>.yaml; alternatif: aim120b_oprf


class Aircraft:
    def __init__(self, key, settings, controller_factory=NeutralControl):
        self.key = key                              # senaryodaki benzersiz anahtar (blue, red)
        self.name = settings["name"]
        self.type = settings["type"]                # C++ hesap profili etiketi; fizik şimdilik F-16
        self.team = settings["team"]
        self.health = 1                             # eski sim: BVRGym healthPoints başlangıcı
        profile = load_missile(MISSILE_PROFILE)
        self.missiles = [Missile(profile, slot) for slot in range(settings["ammo"])]

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

    def state(self):
        """C++'a giden uçak ölçümü (eski sim'le aynı alanlar)."""
        return dict(id=self.key, name=self.name, type=self.type, team=self.team,
                    health=self.health, **self.model.kinematics())
