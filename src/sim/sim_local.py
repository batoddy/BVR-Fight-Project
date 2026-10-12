"""TCP olmadan YAML'daki ilk hedeflerle uçuş ve kayıt.

Workspace içinden: python src/sim/sim_local.py
C++ komutlarıyla çalıştırmak için: python src/sim/sim_main.py
"""
import sys
import time

if __package__:
    from .recorder import Recorder
    from .simulation import Simulation
    from .sim_main import WORKSPACE, load_scenario
else:
    from recorder import Recorder
    from simulation import Simulation
    from sim_main import WORKSPACE, load_scenario


def main():
    scenario = load_scenario()
    simulation = Simulation(scenario, WORKSPACE / "BVRGym")
    commands = simulation.initial_commands()

    with Recorder(WORKSPACE / "recordings", scenario=scenario) as recorder:
        print("Kayıt:", recorder.directory)
        recorder.record(simulation.agents, simulation.commands)

        while not simulation.finished():
            # Yerel komutlarını burada değiştirebilirsin.
            # commands[0]["heading_deg"] = 90
            # simulation.fire("blue", "red")  # Yalnızca açık atış emri.
            simulation.step(commands)
            recorder.record(simulation.agents, simulation.commands)
            if scenario["world"]["sleep_s"] > 0:
                time.sleep(scenario["world"]["sleep_s"])

    print(f"Bitti. {recorder.snapshots} snapshot, {recorder.event_count} olay.")


if __name__ == "__main__":
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8")
    try:
        main()
    except KeyboardInterrupt:
        print("Uçuş durduruldu; kayıtlar kapatıldı.")

