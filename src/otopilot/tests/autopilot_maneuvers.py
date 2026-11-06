"""Gerçek main.exe --maneuver-test akışını TCP ve JSBSim ile uçtan uca doğrula.

.venv/Scripts/python.exe src/otopilot/tests/autopilot_maneuvers.py
Kullanıcının 6000 portunu veya senaryo dosyasını değiştirmez; boş yerel port kullanır.
Kayıt ve loglar tmp/autopilot-maneuver-test/<benzersiz deneme>/ içine yazılır.
"""
import argparse
import contextlib
import copy
import json
import math
from pathlib import Path
import re
import socket
import subprocess
import sys
import uuid

WORKSPACE = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(WORKSPACE))

from src.sim.connector import Message
from src.sim.recorder import Recorder
from src.sim.scenario import load_scenario
from src.sim.simulation import Simulation


def require(condition, label):
    if not condition:
        raise AssertionError(label)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=WORKSPACE / "tmp/autopilot-maneuver-test")
    args = parser.parse_args()
    output = args.output / uuid.uuid4().hex[:8]
    output.mkdir(parents=True)
    scenario = copy.deepcopy(load_scenario())
    scenario["world"].update(duration_s=1200, sleep_s=0)
    with (output / "setup.log").open("w", encoding="utf-8") as log, contextlib.redirect_stdout(log):
        simulation = Simulation(scenario, WORKSPACE / "BVRGym")

    phases = set()
    shots = []
    minimum_altitude_m = float("inf")
    executable = WORKSPACE / "src/otopilot/main.exe"
    with socket.socket() as server, (output / "main.log").open("w", encoding="utf-8") as log:
        server.bind(("127.0.0.1", 0))
        server.listen(1)
        server.settimeout(15)
        port = server.getsockname()[1]
        with subprocess.Popen([str(executable), "--maneuver-test", "127.0.0.1", str(port)],
                              stdout=log, stderr=subprocess.STDOUT) as process:
            try:
                connection, _ = server.accept()
                with connection, connection.makefile("rb") as reader, Recorder(output, scenario=scenario, name="recording") as recorder:
                    connection.settimeout(15)
                    connection.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
                    recorder.record(simulation.agents, simulation.commands)
                    while not simulation.finished():
                        state = simulation.read_state()
                        for aircraft in state["aircraft"]:
                            require(all(math.isfinite(aircraft[key]) for key in ("lat_deg", "lon_deg", "alt_msl_m", "yaw_deg", "tas_mps")),
                                    "non-finite JSBSim measurement")
                            minimum_altitude_m = min(minimum_altitude_m, aircraft["alt_msl_m"])
                            require(aircraft["alt_msl_m"] > 2000, "excessive altitude loss in test sequence")
                        connection.sendall(Message("state", simulation.tick, state).encode())
                        response = Message.decode(reader.readline(1024 * 1024))
                        require(response.kind == "command" and response.tick == simulation.tick, "TCP response tick mismatch")
                        phases.add(response.data["calculations"]["aircraft"][0]["phase"])
                        recorder.record_calculations(response.data["calculations"])
                        for order in response.data["launch"]:
                            require(order == {"id": "red", "target_id": "blue"}, "unexpected test shot")
                            require(simulation.fire(order["id"], order["target_id"]), "test shot rejected")
                            shots.append(dict(time_s=state["sim_time_s"], **order))
                        simulation.step(response.data["aircraft"])
                        recorder.record(simulation.agents, simulation.commands)
                    connection.sendall(Message("end", simulation.tick, {}).encode())
                require(process.wait(timeout=15) == 0, "test autopilot process failed")
            except BaseException:
                process.kill()
                raise
    log_text = (output / "main.log").read_text(encoding="utf-8")
    leg_numbers = {int(value) for value in re.findall(r"step=(\d+)", log_text)}
    require(leg_numbers == set(range(1, 20)), "not every leg and final hold appeared in log")
    require(phases == {0, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 14, 15, 16}, "missing maneuver phase")
    require(len(shots) == 2, "expected exactly two test missiles")
    require("NO_TEST_MISSILE" not in log_text, "test ran without required missile inventory")
    result = dict(steps=simulation.tick, duration_s=1200, phases=sorted(phases), test_legs=18,
                  minimum_altitude_m=minimum_altitude_m, shots=shots, final_hold=True)
    (output / "result.json").write_text(json.dumps(result, indent=2), encoding="utf-8")
    print(json.dumps(result, indent=2), flush=True)
    print(f"Recording and logs: {output}")


if __name__ == "__main__":
    main()
