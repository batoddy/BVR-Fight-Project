"""Simülasyon durumunu oku, uçuş komutlarını uygula ve açık atış emirlerini yürüt.

TCP, kayıt, bekleme veya otomatik taktik karar bu sınıfta bulunmaz.
"""
import math

# Hem doğrudan dosya hem python -m ile çalışsın (sim_main.py ile aynı düzen).
if __package__:
    from .bvrgym_setup import create_agents
    from .scenario import validate_scenario
else:
    from bvrgym_setup import create_agents
    from scenario import validate_scenario

G_LIMIT_MIN, G_LIMIT_MAX = 1.5, 9.0   # g; komuttaki g_limit aralığı (9: F-16 yapısal sınırı)


class Simulation:
    # =========================================================================
    # BAŞLANGIÇ DURUMU VE SİMÜLASYON SAATİ
    # =========================================================================

    def __init__(self, scenario, bvrgym_path):
        validate_scenario(scenario)
        self.world = scenario["world"]
        self.tick = 0
        self.agents = create_agents(scenario, bvrgym_path)

        # Her uçak için son uygulanan hedefler: [yön (derece), irtifa (m), gaz (0..1), g sınırı].
        # g sınırı isteğe bağlı; None ise flight_control.py'nin varsayılanı kullanılır.
        self.commands = {
            key: [settings["spawn"]["heading"], settings["spawn"]["alt"], settings["throttle"], None]
            for key, settings in scenario["agents"].items()
        }
        self.clock = next(iter(self.agents.values())).simObj
        self.start_time = self.clock.get_sim_time_sec()

    def finished(self):
        """İlk ölçümden bu yana geçen fizik süresi senaryo süresine ulaştı mı?"""
        return self.clock.get_sim_time_sec() - self.start_time >= self.world["duration_s"]

    # =========================================================================
    # ÖLÇÜM OKUMA — bu bölüm simülasyonu ilerletmez
    # =========================================================================

    def read_flight_commands(self):
        """Güncel hedefleri mesajdaki liste biçimine çevir; ilk ölçümde YAML hedefleridir."""
        result = []
        for key, values in self.commands.items():
            command = dict(id=key, heading_deg=values[0], altitude_m=values[1], throttle=values[2])
            if values[3] is not None:
                command["g_limit"] = values[3]
            result.append(command)
        return result

    @staticmethod
    def _kinematics(model):
        """BVRGym/JSBSim değerleri; mesafe veya açı hesabı yapılmaz."""
        return dict(
            lat_deg=float(model.fdm["position/lat-geod-deg"]),
            lon_deg=model.get_long_gc_deg(), alt_msl_m=model.get_altitude(),
            yaw_deg=model.get_psi(), pitch_deg=model.get_theta(), roll_deg=model.get_phi(),
            tas_mps=model.get_true_airspeed(), mach=model.get_mach(),
            velocity_ned_mps=[model.get_v_north(), model.get_v_east(), model.get_v_down()],
        )

    def read_state(self):
        """Bütün uçak ve füzeleri oku; simülasyonu ilerletme."""
        aircraft, missiles = [], []
        for key, agent in self.agents.items():
            aircraft.append(dict(
                id=key, name=agent.conf.agent_name, type=agent.conf.aircraft_type,
                team=agent.conf.team,
                health=agent.healthPoints, **self._kinematics(agent.simObj),
            ))
            for ammo_id, missile in agent.ammo.items():
                ready = missile.is_ready_to_launch()
                state = "ready" if ready else "in_flight" if missile.is_active() else "ended"
                target_id = next((other_key for other_key, other in self.agents.items()
                                  if other is missile.target), None)
                missiles.append(dict(
                    id=f"{key}/missile/{ammo_id}", owner_id=key, target_id=target_id,
                    state=state,
                    outcome="hit" if missile.target_hit else "lost" if missile.target_lost else None,
                    flight_time_s=None if ready else missile.get_sim_time_sec(),
                    kinematics=None if ready else self._kinematics(missile),
                ))
        return dict(tick=self.tick, sim_time_s=self.clock.get_sim_time_sec(),
                    aircraft=aircraft, missiles=missiles,
                    flight_commands=self.read_flight_commands())

    # =========================================================================
    # UÇUŞ KOMUTUNU DOĞRULAMA VE BİR ADIM İLERLETME
    # =========================================================================

    def step(self, aircraft_commands):
        """Bütün uçakların komutlarını doğrula; sonra hepsini bir adım ilerlet."""
        if self.finished():
            raise ValueError("Simülasyon süresi doldu.")
        commands = self._validate_flight_commands(aircraft_commands)

        # Bütün komutlar doğrulanmadan hiçbir uçağı ilerletme.
        # g sınırı BVRGym'in apply_action yolundan geçmez; uçağın kontrolcüsüne doğrudan yazılır.
        for key, agent in self.agents.items():
            heading_deg, altitude_m, throttle, g_limit = commands[key]
            agent.simObj.aircraft_control.g_limit = g_limit
            agent.apply_action([heading_deg, altitude_m, throttle])

        self.commands = commands
        self.tick += 1

    def _validate_flight_commands(self, aircraft_commands):
        """Her uçak için tek, eksiksiz komut iste; henüz fizik durumunu değiştirme."""
        if not isinstance(aircraft_commands, list):
            raise ValueError("aircraft komutları liste olmalı.")

        commands = {}
        for item in aircraft_commands:
            if not isinstance(item, dict) or not isinstance(item.get("id"), str):
                raise ValueError("Her komutta uçak id alanı bulunmalı.")
            key = item["id"]
            if key not in self.agents or key in commands:
                raise ValueError(f"Bilinmeyen veya tekrarlanan uçak id: {key}")
            values = [item.get("heading_deg"), item.get("altitude_m"), item.get("throttle")]
            if any(type(value) not in (int, float) or not math.isfinite(value) for value in values):
                raise ValueError(f"{key}: heading, altitude ve throttle sonlu sayı olmalı.")
            if not 0 <= values[0] <= 360 or not 0 <= values[2] <= 1:
                raise ValueError(f"{key}: heading 0..360, throttle 0..1 aralığında olmalı.")
            # İsteğe bağlı g sınırı: dönüşte çekilecek en büyük yük (out sert, crank/beam yumuşak).
            g_limit = item.get("g_limit")
            if g_limit is not None:
                if type(g_limit) not in (int, float) or not math.isfinite(g_limit):
                    raise ValueError(f"{key}: g_limit sonlu sayı olmalı.")
                if not G_LIMIT_MIN <= g_limit <= G_LIMIT_MAX:
                    raise ValueError(f"{key}: g_limit {G_LIMIT_MIN}..{G_LIMIT_MAX} aralığında olmalı.")
            commands[key] = values + [g_limit]
        if set(commands) != set(self.agents):
            raise ValueError("Her uçak için tam bir komut gerekli.")

        return commands

    # =========================================================================
    # AÇIK FÜZE ATIŞ EMRİ
    # =========================================================================

    # -------------------------------------------------------------------------
    # BVRGym'in ORİJİNAL atış yolu (değiştirilmedi; biz onun yerine fire() kullanıyoruz)
    #
    # BVRGym/jsb_gym/agents/agents.py → BaseBVRAgent.launch_missile():
    #
    #     def launch_missile(self):
    #         if not self.is_own_missile_active():          # havada kendi füzesi varsa hiç atmaz
    #             for i in self.ammo:
    #                 if self.ammo[i].is_ready_to_launch():
    #                     self.ammo[i].set_target(self.target)
    #                     self.ammo[i].launch(self)
    #                     break
    #
    # BVRGym/jsb_gym/simObjects/missiles.py → AAMBVR.launch():
    #
    #     def launch(self, carrier_aircraft):
    #         if self.is_alive():
    #             self.active = True
    #             lat = carrier_aircraft.simObj.get_lat_gc_deg()
    #             long = carrier_aircraft.simObj.get_long_gc_deg()
    #             alt = carrier_aircraft.simObj.get_altitude()
    #             vel = carrier_aircraft.simObj.get_mach()        # Mach veriliyor ...
    #             heading = carrier_aircraft.simObj.get_psi()
    #
    #             self.reset(lat, long, alt, vel, heading)       # ... ama reset() m/s bekliyor
    #
    # Farklarımız:
    #   1. is_own_missile_active() koruması yok: C++ atış emri verdiyse havada füze olsa da atılır.
    #   2. Hedef BVRGym'in agent.target'ından değil, C++'ın target_id'sinden gelir.
    #   3. launch() sonrası füze aynı konum/yönde get_true_airspeed() (m/s) ile yeniden reset edilir;
    #      orijinalde Mach sayısı m/s sanılıyordu: Mach 0.9 ile atılan füze 0.9 m/s ile başlıyordu.
    # -------------------------------------------------------------------------

    def fire(self, shooter_id, target_id):
        """Hazır ilk füzeyi belirtilen hedefe at; uygulanırsa True, atılamazsa False."""
        shooter, target = self.agents[shooter_id], self.agents[target_id]
        if shooter is target:
            return False
        missile = next((m for m in shooter.ammo.values() if m.is_ready_to_launch()), None)
        if missile is None:
            return False

        # C++ açık atış emri verdiği için havadaki başka bir füze yeni atışı engellemez.
        # BVRGym'in launch_missile() koruması yerine hazır slotu doğrudan kullanırız.
        shooter.set_target(target)
        missile.set_target(target)
        missile.launch(shooter)

        # BVRGym launch() Mach veriyor, reset() m/s bekliyor. Aynı konum ve yönde
        # gerçek hava hızıyla yeniden başlatıp mevcut birim hatasını düzeltiriz.
        aircraft = shooter.simObj
        missile.reset(aircraft.get_lat_gc_deg(), aircraft.get_long_gc_deg(), aircraft.get_altitude(),
                      aircraft.get_true_airspeed(), aircraft.get_psi())
        return True
