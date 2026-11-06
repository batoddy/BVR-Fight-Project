"""C++ manevralarını gerçek JSBSim ölçüm/komut döngüsünde dene.

Workspace: .venv/Scripts/python.exe src/otopilot/tests/jsbsim_maneuvers.py
Önce cmake --build src/otopilot/build --target maneuver_probe çalıştırılır.
Normal senaryo dosyasını, otopilot seçimini ve BVRGym kaynaklarını değiştirmez.
"""
import argparse
import copy
import contextlib
import json
import math
from pathlib import Path
import subprocess
import sys

WORKSPACE = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(WORKSPACE))

from src.sim.scenario import load_scenario
from src.sim.simulation import Simulation


def require(condition, label):
    if not condition:
        raise AssertionError(label)


def run_case(probe, mode, angle_deg, duration_s, output):
    scenario = copy.deepcopy(load_scenario())
    scenario["world"].update(duration_s=duration_s, sleep_s=0, sim_time_step=1)
    for settings in scenario["agents"].values():
        settings["ammo"] = 1
        settings["spawn"].update(lat=59.0, long=18.0, alt=10000, vel=300, heading=0)
    scenario["agents"]["red"]["spawn"]["lat"] = 59.55
    if mode == "flank":
        scenario["agents"]["blue"]["spawn"]["lat"] = 59.1
        scenario["agents"]["red"]["spawn"]["lat"] = 59.0
    if mode in ("collision", "pure", "lag", "lead"):
        scenario["agents"]["red"]["spawn"].update(heading=90, vel=250)
        scenario["agents"]["blue"]["spawn"]["vel"] = 350
    if mode in ("notch", "drag", "recommit", "out"):
        scenario["agents"]["red"]["spawn"]["heading"] = 180
    if mode == "recommit":
        scenario["agents"]["blue"]["spawn"]["heading"] = 150

    log_path = output / f"{mode}_{angle_deg:g}.log"
    with log_path.open("w", encoding="utf-8") as log, contextlib.redirect_stdout(log):
        simulation = Simulation(scenario, WORKSPACE / "BVRGym")
    statuses = dict(running=0, satisfied=0, done=0)
    min_altitude_m = float("inf")
    max_g_command = 0
    threat_seen = False
    last = None
    with subprocess.Popen([str(probe)], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                          stderr=subprocess.PIPE, text=True, encoding="utf-8") as process:
        try:
            while not simulation.finished():
                state = simulation.read_state()
                request = dict(state=state, mode=mode, angle_deg=angle_deg,
                               fire_red=mode in ("notch", "drag") and simulation.tick == 0)
                process.stdin.write(json.dumps(request, allow_nan=False) + "\n")
                process.stdin.flush()
                line = process.stdout.readline()
                if not line:
                    raise RuntimeError("C++ probe stopped: " + process.stderr.read())
                last = json.loads(line)
                statuses[last["status"]] += 1
                threat_seen |= last["metrics"]["has_threat"]
                for item in last["launch"]:
                    require(simulation.fire(item["id"], item["target_id"]), "test missile launch rejected")
                for command in last["aircraft"]:
                    require(all(math.isfinite(command[key]) for key in ("heading_deg", "altitude_m", "throttle", "g_limit")),
                            f"{mode}: non-finite or missing flight command")
                    require(1.5 <= command["g_limit"] <= 9, f"{mode}: invalid g limit")
                    max_g_command = max(max_g_command, command["g_limit"])
                simulation.step(last["aircraft"])
                for aircraft in simulation.read_state()["aircraft"]:
                    require(all(math.isfinite(aircraft[key]) for key in ("lat_deg", "lon_deg", "alt_msl_m", "yaw_deg", "tas_mps")),
                            f"{mode}: non-finite flight state")
                    min_altitude_m = min(min_altitude_m, aircraft["alt_msl_m"])
                    require(aircraft["alt_msl_m"] > 2000, f"{mode}: excessive altitude loss")
            process.stdin.close()
            require(process.wait(timeout=10) == 0, process.stderr.read())
        except BaseException:
            process.kill()
            raise

    require(sum(statuses.values()) > 0, "empty physics run")
    if mode in ("notch", "drag"):
        require(threat_seen, f"{mode}: no real missile threat was observed")
    if mode in ("hold", "fly_heading", "offset", "crank", "beam", "flank", "commit", "pure", "lag", "lead", "collision"):
        require(statuses["satisfied"] > 0, f"{mode}: did not satisfy its condition; last metrics {last['metrics']}")
    if mode in ("out", "recommit"):
        require(statuses["done"] > 0, f"{mode}: did not reach done")
    return dict(mode=mode, angle_deg=angle_deg, duration_s=duration_s, steps=sum(statuses.values()),
                statuses=statuses, min_altitude_m=min_altitude_m, max_g_command=max_g_command,
                threat_seen=threat_seen, last_metrics=last["metrics"])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--probe", type=Path, default=WORKSPACE / "src/otopilot/build/maneuver_probe.exe")
    parser.add_argument("--output", type=Path, default=WORKSPACE / "tmp/maneuver-validation")
    parser.add_argument("--mode", help="Yalnız bu manevrayı dene")
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    cases = [("hold", 0, 60), ("fly_heading", 60, 60), ("offset", 40, 60), ("offset", -40, 60),
             ("commit", 0, 60), ("crank", 0, 60), ("beam", 0, 90), ("flank", 40, 150),
             ("flank", -40, 150), ("pure", 0, 60), ("lag", 0, 60), ("lead", 0, 60),
             ("collision", 0, 180), ("out", 0, 90), ("recommit", 0, 60),
             ("notch", 0, 45), ("drag", 0, 45)]
    if args.mode:
        cases = [case for case in cases if case[0] == args.mode]
        require(bool(cases), "unknown mode")
    results = []
    for mode, angle, duration in cases:
        result = run_case(args.probe.resolve(), mode, angle, duration, args.output)
        results.append(result)
        print(f"{mode} {angle:g}: {result['steps']} steps, {result['statuses']}", flush=True)
    destination = args.output / "results.json"
    destination.write_text(json.dumps(results, indent=2), encoding="utf-8")
    print(f"Physics checks passed. Results: {destination}")


if __name__ == "__main__":
    main()
