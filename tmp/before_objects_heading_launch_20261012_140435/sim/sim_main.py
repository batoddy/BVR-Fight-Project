"""YAML, simülasyon, TCP bağlantısı ve kaydı birleştiren giriş dosyası.

Workspace içinden: python src/sim/sim_main.py
Python ölçüm gönderir, C++ komutunu bekler, ardından bir adım ilerler.
"""
import sys
import time
from pathlib import Path

import yaml

# Hem doğrudan dosya hem python -m ile çalıştırılabilir.
if __package__:
    from .connector import Message, TCPConnection
    from .recorder import Recorder
    from .simulation import Simulation
else:
    from connector import Message, TCPConnection
    from recorder import Recorder
    from simulation import Simulation

SIM_DIRECTORY = Path(__file__).resolve().parent
WORKSPACE = SIM_DIRECTORY.parents[1]  # Workspace/src/sim -> Workspace
SCENARIO_PATH = SIM_DIRECTORY / "scenario.yaml"


def load_scenario():
    with SCENARIO_PATH.open(encoding="utf-8-sig") as file:
        return yaml.safe_load(file)


def main():
    scenario = load_scenario()
    simulation = Simulation(scenario, WORKSPACE / "BVRGym")
    network = scenario["connection"]

    with Recorder(WORKSPACE / "recordings", scenario=scenario) as recorder:
        print("Kayıt:", recorder.directory, flush=True)
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

                # 3. Komutları uygula ve oluşan durumu kaydet.
                simulation.step(command.data.get("aircraft"))
                recorder.record(simulation.agents, simulation.commands)

                # 4. Yalnızca YAML'daki gerçek zaman beklemesi.
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
