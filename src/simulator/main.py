"""Senaryo, simülasyon, TCP bağlantısı ve kaydı birleştiren giriş dosyası.

Workspace içinden: python src/simulator/main.py [--name ad] [--altitude m] [--throttle 0..1] [--range NM] [--duration s]
Python ölçüm gönderir, C++ komutunu bekler, sonra bir tick ilerler (akış eski src/sim/sim_main.py ile aynı).
"""
import argparse
import math
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))   # src/ : "simulator" paketini bulmak için

from simulator.connector import Message, TCPConnection
from simulator.flight_control.aml import AMLController
from simulator.recorder import Recorder
from simulator.scenario import WORKSPACE, load_scenario
from simulator.simulation import Simulation


def read_arguments():
    parser = argparse.ArgumentParser(description="Simülatörü C++ otopilotla çalıştır.")
    parser.add_argument("--name", help="Kayıt klasörü adı (recordings/<name>); yoksa tarih-saat.")
    parser.add_argument("--altitude", type=float, help="İki uçağın başlangıç irtifası, MSL metre.")
    parser.add_argument("--throttle", type=float, help="İki uçağın başlangıç gazı, 0..1.")
    parser.add_argument("--range", type=float, help="Başlangıç mesafesi, NM; red, blue'nun bu kadar kuzeyine konur.")
    parser.add_argument("--duration", type=float, help="Simülasyon süresi, s; YAML world.duration_s yerine.")
    arguments = parser.parse_args()
    if arguments.throttle is not None and not 0.0 <= arguments.throttle <= 1.0:
        parser.error("--throttle 0..1 aralığında olmalı.")
    if arguments.range is not None and arguments.range <= 0.0:
        parser.error("--range pozitif olmalı.")
    if arguments.duration is not None and (not math.isfinite(arguments.duration) or arguments.duration <= 0):
        parser.error("--duration pozitif ve sonlu olmalı.")
    return arguments


def apply_arguments(scenario, arguments):
    """Komut satırı değerlerini senaryonun üstüne yaz."""
    if arguments.duration is not None:
        scenario["world"]["duration_s"] = arguments.duration
    for settings in scenario["agents"].values():
        if arguments.altitude is not None:
            settings["spawn"]["alt"] = arguments.altitude   # ilk irtifa komutu da spawn'dan: uçak bu irtifayı korur
        if arguments.throttle is not None:
            settings["throttle"] = arguments.throttle
    if arguments.range is not None:
        agents = scenario["agents"]
        agents["red"]["spawn"]["lat"] = agents["blue"]["spawn"]["lat"] + arguments.range / 60.0   # 1 NM ≈ 1/60° enlem


def main():
    arguments = read_arguments()
    scenario = load_scenario()
    apply_arguments(scenario, arguments)
    simulation = Simulation(scenario, controller_factory=AMLController)   # NASA AML uçuş kontrolü (flight_control/TASARIM.md)
    network = scenario["connection"]

    with Recorder(WORKSPACE / "recordings", scenario=scenario, name=arguments.name) as recorder:
        print("Kayıt:", recorder.directory, flush=True)
        recorder.record(simulation)                                   # ilk ölçüm: C++ ilk kararını buna göre verir
        with TCPConnection(network["host"], network["port"]) as connection:
            while not simulation.finished():
                connection.send(Message("state", simulation.tick, simulation.read_state()))
                command = connection.receive()                        # C++ cevabını bekle; bu sırada fizik durur
                if command.kind != "command" or command.tick != simulation.tick:
                    raise ValueError("Komut türü veya adım numarası yanlış.")
                recorder.record_calculations(command.data.get("calculations"))   # gönderilen state'in zamanıyla
                for order in command.data.get("launch", []):
                    simulation.fire(order["id"], order["target_id"])
                simulation.step(command.data.get("aircraft"))
                recorder.record(simulation)
                if scenario["world"]["sleep_s"] > 0:
                    time.sleep(scenario["world"]["sleep_s"])
            connection.send(Message("end", simulation.tick, {}))
    print(f"Bitti. {recorder.snapshots} snapshot, {recorder.event_count} olay.")


if __name__ == "__main__":
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8")
    try:
        main()
    except KeyboardInterrupt:
        print("Simülasyon durduruldu; kayıtlar kapatıldı.")
    except (ConnectionError, OSError, ValueError) as error:
        sys.exit(f"Simülasyon durdu: {error}")
