"""YAML, simülasyon, TCP bağlantısı ve kaydı birleştiren giriş dosyası.

Workspace içinden: python src/sim/sim_main.py
Python ölçüm gönderir, C++ komutunu bekler, ardından bir adım ilerler.

İsteğe bağlı parametreler (verilmeyen YAML'daki değerle kalır):
  python src/sim/sim_main.py --name deneme1 --altitude 8000 --throttle 0.9 --range 20
"""
import argparse
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


def read_arguments():
    parser = argparse.ArgumentParser(description="BVRGym simülasyonunu C++ otopilotla çalıştır.")
    parser.add_argument("--name", help="Kayıt klasörünün adı (recordings/<name>). Yoksa tarih-saat.")
    parser.add_argument("--altitude", type=float, help="İki uçağın da başlangıç irtifası, MSL metre.")
    parser.add_argument("--throttle", type=float, help="İki uçağın da başlangıç gaz komutu, 0..1.")
    parser.add_argument("--range", type=float, help="Başlangıç mesafesi, NM; red, blue'nun bu kadar kuzeyine konur.")
    arguments = parser.parse_args()
    if arguments.throttle is not None and not 0.0 <= arguments.throttle <= 1.0:
        parser.error("--throttle 0..1 aralığında olmalı.")
    if arguments.range is not None and arguments.range <= 0.0:
        parser.error("--range pozitif olmalı.")
    return arguments


def main():
    arguments = read_arguments()

    # Kurulum tek sefer yapılır; fizik yalnız simulation.step() ile ilerler.
    scenario = load_scenario()

    # Verilen irtifa her uçağın YAML'daki spawn.alt'ının yerine geçer. İlk irtifa
    # komutu da spawn.alt'tan alındığı için uçaklar bu irtifayı korur.
    if arguments.altitude is not None:
        for settings in scenario["agents"].values():
            settings["spawn"]["alt"] = arguments.altitude

    # Verilen gaz her uçağın YAML'daki throttle'ının yerine geçer.
    if arguments.throttle is not None:
        for settings in scenario["agents"].values():
            settings["throttle"] = arguments.throttle

    # Verilen mesafe: red, blue ile aynı boylamda, 1 NM ≈ 1/60 derece enlem kuzeyine konur.
    if arguments.range is not None:
        agents = scenario["agents"]
        agents["red"]["spawn"]["lat"] = agents["blue"]["spawn"]["lat"] + arguments.range / 60.0

    simulation = Simulation(scenario, WORKSPACE / "BVRGym")
    network = scenario["connection"]

    with Recorder(WORKSPACE / "recordings", scenario=scenario, name=arguments.name) as recorder:
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

# Kurulumdan uçuşa çağrı izi ve BVRGym'e yapılan müdahaleler: src/sim/README.md
