"""YAML, simülasyon, TCP bağlantısı ve kaydı birleştiren giriş dosyası.

Workspace içinden: python src/sim/sim_main.py
Python ölçüm gönderir, C++ komutunu bekler, ardından bir adım ilerler.
"""
import sys
import time

# Hem doğrudan dosya hem python -m ile çalıştırılabilir.
if __package__:
    from .connector import Message, TCPConnection
    from .recorder import Recorder
    from .scenario import WORKSPACE, load_scenario
    from .simulation import Simulation
else:
    from connector import Message, TCPConnection
    from recorder import Recorder
    from scenario import WORKSPACE, load_scenario
    from simulation import Simulation

# =============================================================================
# ANA AKIŞ — kurulum → ölçüm → komut → ilerletme → kayıt
# =============================================================================


def main():
    # Kurulum tek sefer yapılır; fizik yalnız simulation.step() ile ilerler.
    scenario = load_scenario()
    simulation = Simulation(scenario, WORKSPACE / "BVRGym")
    network = scenario["connection"]

    with Recorder(WORKSPACE / "recordings", scenario=scenario) as recorder:
        print("Kayıt:", recorder.directory, flush=True)
        # İlk ölçümü de kaydet: C++ ilk kararını bu durum üzerinden verecek.
        recorder.record(simulation.agents, simulation.commands)

        with TCPConnection(network["host"], network["port"]) as connection:
            while not simulation.finished():
                # 1. Mevcut ölçümü gönder. Beklerken uçaklar ilerlemez.
                state = simulation.read_state()
                connection.send(Message("state", simulation.tick, state))

                # 2. C++ hesabının sonucunu bekle ve adım numarasını kontrol et.
                command = connection.receive()
                if command.kind != "command" or command.tick != simulation.tick:
                    raise ValueError("Komut türü veya adım numarası yanlış.")

                # C++ hesapları gönderdiğimiz state'e ait: adımdan önce, o ölçümün zamanıyla yaz.
                recorder.record_calculations(command.data.get("calculations"))

                # 3. Önce atış emirleri, sonra uçuş komutları; oluşan durumu kaydet.
                # fire() hazır füze yoksa atmaz; havada füze olması engel değil.
                for order in command.data.get("launch", []):
                    simulation.fire(order["id"], order["target_id"])
                simulation.step(command.data.get("aircraft"))
                recorder.record(simulation.agents, simulation.commands)

                # 4. Yalnızca YAML'daki gerçek zaman beklemesi.
                if scenario["world"]["sleep_s"] > 0:
                    time.sleep(scenario["world"]["sleep_s"])

            connection.send(Message("end", simulation.tick, {}))

    print(f"Bitti. {recorder.snapshots} snapshot, {recorder.event_count} olay.")


# =============================================================================
# DOSYAYI DOĞRUDAN ÇALIŞTIRMA VE KAPANIŞ
# =============================================================================

if __name__ == "__main__":
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8")
    try:
        main()
    except KeyboardInterrupt:
        print("Simülasyon durduruldu; kayıtlar kapatıldı.")
    except (ConnectionError, OSError, ValueError) as error:
        sys.exit(f"Simülasyon durdu: {error}")


# =============================================================================
# SİMÜLASYONUN TEMEL AKIŞI
# Bu bölüm, kurulumdan uçuşa kadar çağrıların izini gösterir.
# =============================================================================
#
# KURULUM — simülasyon başlarken bir kez yapılır
# -----------------------------------------------------------------------------
#
# 1. Senaryo dosyası okunur: süre, adım ayarı ve uçakların başlangıç bilgileri.
#    Çağrı    : scenario = load_scenario()
#    Bulunduğu: src/sim/scenario.py → load_scenario()
#    Veri     : src/sim/scenario.yaml
#
# 2. Simülasyon oluşturulur; temel ayarlar kontrol edilir, ajan kurulumu başlar.
#    Çağrı    : simulation = Simulation(scenario, WORKSPACE / "BVRGym")
#    Bulunduğu: src/sim/simulation.py → Simulation.__init__()
#    İçeride  : validate_scenario() → src/sim/scenario.py
#               create_agents()   → src/sim/bvrgym_setup.py
#
# 3. Her uçak için config kopyalanır; uçak ve füze slotları oluşturulur.
#    Çağrı    : _create_agent(settings, sim_time_step)
#    Bulunduğu: src/sim/bvrgym_setup.py → _create_agent()
#    İçeride  : _copy_config(f16_config), _copy_config(AAM_config)
#               Uçak/füze adım ayarları ve mevcut füze dalış ayarı uygulanır.
#               BaseBVRAgent(conf, None) ile BVRGym ajanı oluşturulur.
#    Devamı   : BVRGym/jsb_gym/agents/agents.py → BaseBVRAgent.__init__()
#               F16BVR(...)  → uçak fizik modeli oluşturulur.
#               reset_agent() → başlangıç konumu, irtifası, hızı ve yönü verilir.
#               reset_ammo()  → mühimmat sayısı kadar AAMBVR füzesi oluşturulur.
#
# 4. Uçağın yön/irtifa kontrolü bağlanır; ilk komutlar ve başlangıç saati tutulur.
#    Çağrı    : flight_control.install(agent.simObj.aircraft_control)
#    Bulunduğu: src/sim/flight_control.py → install()
#    Devamı   : src/sim/simulation.py → Simulation.__init__()
#               self.commands   → YAML'daki yön, irtifa ve gaz hedefleri.
#               self.start_time → simülasyonun başlangıç saati.
#
#
# UÇUŞ DÖNGÜSÜ — süre dolana kadar aşağıdaki adımlar tekrarlanır
# -----------------------------------------------------------------------------
#
# 5. Süre kontrol edilir; devam edilecekse mevcut uçak ve füze durumları okunur.
#    Çağrı    : simulation.finished(), simulation.read_state()
#    Bulunduğu: src/sim/simulation.py → Simulation.finished(), read_state()
#    İçeride  : _kinematics() ile konum, yönelim ve hız değerleri okunur.
#               Okuma sırasında uçaklar ve füzeler ilerletilmez.
#               Bu durum üzerinden sonraki yön/irtifa/gaz ve atış emirleri gelir.
#
# 6. Atış emri varsa hazır ilk füze hedefe atanır ve fırlatılır.
#    Çağrı    : simulation.fire(shooter_id, target_id)
#    Bulunduğu: src/sim/simulation.py → Simulation.fire()
#    İçeride  : missile.set_target(target), missile.launch(shooter)
#               Füzenin başlangıç hızı uçağın gerçek hava hızıyla düzeltilir.
#    Devamı   : BVRGym/jsb_gym/simObjects/missiles.py → AAMBVR.launch()
#
# 7. Bütün uçakların uçuş komutları doğrulanır; sonra ajanlar sırayla ilerletilir.
#    Çağrı    : simulation.step(aircraft_commands)
#    Bulunduğu: src/sim/simulation.py → Simulation.step()
#    İçeride  : _validate_flight_commands() → yön/irtifa/gaz komutlarını kontrol et.
#               agent.apply_action(...)   → her uçağa kendi komutunu uygula.
#    Devamı   : BVRGym/jsb_gym/agents/agents.py → BaseBVRAgent.apply_action()
#               Önce o uçağı, ardından o uçağın aktif füzelerini ilerletir.
#
# 8. Uçak uçurulur: hedefler kontrol yüzeylerine çevrilir, fizik hesaplanır.
#    Çağrı    : agent.simObj.step([heading_deg, altitude_m, throttle])
#    Bulunduğu: BVRGym/jsb_gym/simObjects/aircraft.py → F16BVR.step(), _step()
#    İçeride  : get_control_input(...) → src/sim/flight_control.py → _control()
#               _set_bank() yatışı, _set_elevator() yük faktörünü kontrol eder.
#               Büyük irtifa hatasında BVRGym'in kendi kontrolü kullanılır.
#               command_aircraft(...) ile yüzey ve gaz komutları uygulanır.
#               self.fdm.run() ile JSBSim bir fizik adımı hesaplar.
#
# 9. Aktif füzeler uçurulur: hedef takibi, güdüm ve füze fiziği ilerletilir.
#    Çağrı    : agent.apply_missile_action() → missile.step()
#    Bulunduğu: BVRGym/jsb_gym/agents/agents.py → BaseBVRAgent.apply_missile_action()
#    Devamı   : BVRGym/jsb_gym/simObjects/missiles.py → AAMBVR.step(), _step()
#    İçeride  : Düşük hız, düşük irtifa ve isabet koşulları değerlendirilir.
#               İlk ivmelenme aşamasından sonra TAS 200 m/s altındaysa uçuş sonlanır.
#               PN.get_guidance(...) ile yön/irtifa hedefi hesaplanır.
#               command_missile(...) ve self.fdm.run() ile füze ilerletilir.
#
# 10. Son uygulanan komutlar saklanır ve dış adım sayacı bir artırılır.
#     Bulunduğu: src/sim/simulation.py → Simulation.step() sonu
#     İçeride  : self.commands = commands, self.tick += 1
#               5. adıma dönülür; süre dolmuşsa döngü tamamlanır.
#
# Not: step() birden fazla JSBSim fizik alt adımı çalıştırabilir.
#      tick dış döngü sayacıdır; geçen süre fizik modelinin saatinden okunur.
# =============================================================================


# =============================================================================
# BVRGYM'İN ORİJİNAL DAVRANIŞINA YAPILAN MÜDAHALELER
# Doğrudan BVRGym kaynak değişiklikleri ile dışarıdan bağlanan uyarlamalar.
# =============================================================================
#
# 1. UÇUŞ KONTROLÜ — dışarıdan, oluşturulan her uçak nesnesine bağlanır.
#    Yer: src/sim/flight_control.py → install(), _control()
#    AircraftPIDAutopilot.get_control_input yerine bizim kontrolümüz çalışır.
#    Yön/irtifa hatası yatış ve hedef yük faktörüne çevrilir; elevator PI kontrolü
#    ile uygulanır. Sert dönüş hedefi 6 g, yük faktörü sınırları 0,5..6 g'dir.
#    İrtifa hatası 1500 m'yi aşınca orijinal get_control_input çağrılır.
#
# 2. ATIŞ — BVRGym'in launch_missile() yolu yerine hazır slot doğrudan kullanılır.
#    Yer: src/sim/simulation.py → Simulation.fire()
#    BaseBVRAgent.launch_missile() havada kendi füzesi varken yeni atışı engeller.
#    Biz C++'tan açık atış emri gelince ilk hazır füzeye hedef atayıp launch()
#    çağırırız; havada başka bir füzenin bulunması yeni atışı engellemez.
#    Ajan olarak BaseBVRAgent kullanılır; atış kararını C++ verir.
#
# 3. ATIŞTA HIZ BİRİMİ — launch() sonrasında başlangıç durumu düzeltilir.
#    Yer: src/sim/simulation.py → Simulation.fire()
#    Orijinal AAMBVR.launch(), reset()'e hız olarak Mach verir; reset() m/s bekler.
#    Bu nedenle füze aynı konum ve yönde, taşıyıcının get_true_airspeed() değeriyle
#    yeniden reset edilir. Düzeltme Simulation.fire() yolunda uygulanır.
#
# 4. KURULUM AYARLARI — orijinal config'lerin ajan başına kopyalarına uygulanır.
#    Yer: src/sim/bvrgym_setup.py → _copy_config(), _create_agent()
#    Uçak ve füzenin Sim_time_step değeri scenario.yaml'dan alınır.
#    Füzenin dive_at değeri 15 km yerine 25 km yapılır; hedefe yaklaşırken seyir
#    irtifasından hedefe dalışa daha erken geçmesi amaçlanır.
#    data_output_xml ve fg_sleep_time kapatılır. Kayıt Recorder ile, gerçek zaman
#    beklemesi sim_main.py içindeki world.sleep_s ile yönetilir.
#
# 5. FÜZENİN SONLANMASI — BVRGym kaynak dosyaları doğrudan değiştirildi.
#    Yer: BVRGym/jsb_gym/simObjects/missiles.py → AAMBVR.step()
#    Orijinalde ilk ivmelenme aşamasından sonra Mach 1 altına düşen füze kayıp
#    sayılıyordu. Bu kontrol is_speed_low() ile gerçek hava hızına (m/s) çevrildi.
#    target_lost_below_mach yerine target_lost_below_speed_mps alanı kullanılır:
#      BVRGym/data_classes/missile_dataclass.py → MissilePerformance
#      BVRGym/jsb_gym/simObjects/config/AAM_config.py → missile_performance
#    Hız eşiği 200 m/s'dir; orijinal ilk ivmelenme aşaması koruması sürer.
#    Mevcut irtifa eşiği 0 m'dir:
#    get_altitude() < target_lost_below_alt olduğunda kayıp sayılır.
#    İsabet kontrolü de uçuşu sonlandırır; mevcut etkin yarıçap 300 m'dir.
#    Hız/irtifa sınırı veya isabet oluşunca step() hemen döner; kalan fizik alt
#    adımları çalıştırılmaz. Hız ve irtifa kontrolleri isabetten önce yapılır.
#    is_target_lost() çağrısı orijinalde de yorum satırıydı; şu anda da kapalıdır.
#
# 6. DIŞ KONTROL DÖNGÜSÜ — Python ölçüm gönderir, C++ kararını bekleyip ilerler.
#    Yer: src/sim/sim_main.py, simulation.py, connector.py, recorder.py
#    Uçuş ve atış emirleri TCP üzerinden gelir; durum okuma fiziği ilerletmez.
#    BVRGym apply_action() çağrısı önce uçağı, sonra onun aktif füzelerini uçurur.
#    JSBSim uçak/füze XML'leri ve BVRGym'in PN güdümü bu akışta kullanılır.
# =============================================================================
