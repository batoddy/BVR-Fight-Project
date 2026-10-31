"""WEZ (Raero) ve R_cold taraması: füzenin hedefe ERİŞİMİ ölçülür, isabeti değil.

Raero (BMS AMRAAM DLZ): azami kinematik menzil; hedef manevra yapmaz, füze kusursuz kabul edilir.
Erişim: füze hâlâ uçarken hedefi geçti (hedef artık füzenin gidiş yönünde önünde değil: yanında ya da
arkasında), ya da BVRGym isabet verdi. Iskalama mesafesine bakılmaz. BVRGym füzeyi Mach 1'in altında
kaybettiği için "hâlâ uçuyor" = enerjisi var. Hedef füzeden hızlı kaçıyorsa hep önünde kalır: erişim yok.

v2: v1 "mesafe büyümeye başladı" = geçti sayıyordu; hızlı kaçan hedefte füze yavaşlayınca mesafe
büyüdüğü için yanlış "erişti" verdi. v1 dosyaları (wez_olcumler_*.csv, wez_kontrol.csv) bu yüzden
geçersiz, kayıt için duruyor; v2 sonuçları *_v2.csv dosyalarına yazılır.

Senaryo: red (atan) kuzeyde, güneye uçar; blue (hedef) güneyde. Füze ilk adımda atılır, kimse manevra yapmaz.
  aspect hot : blue kuzeye (red'e doğru) uçar
  aspect beam: blue doğuya (red'e dik) uçar
  aspect cold: blue güneye (red'den kaçar) uçar  -> R_cold ölçümünün aynısı, yeni erişim kriteriyle

Gruplar:
  ana       : irtifa 4/6/8/10 km x blue hızı 220/300/380 m/s x blue gazı 0.3/0.6/1.0; red 300 m/s, gaz 0.45
              (r_cold_formulu.md ile aynı ızgara)
  atan_hizi : 8 km, red hızı 220/300/380 m/s; blue 300 m/s, gaz 0.6

Sınır ikiye bölme ile bulunur (1-150 NM, 0.15 NM hassasiyet). Sonuçlar senaryo bitince CSV'ye eklenir;
betik yarıda kesilirse yeniden çalıştırınca biten senaryoları atlar.

Kullanım (Workspace klasöründen):
  .venv/Scripts/python.exe -X utf8 src/otopilot/calculations/wez_grafikler/wez_tarama.py --aspect hot
  .venv/Scripts/python.exe -X utf8 src/otopilot/calculations/wez_grafikler/wez_tarama.py --aspect beam
  .venv/Scripts/python.exe -X utf8 src/otopilot/calculations/wez_grafikler/wez_tarama.py --aspect cold
  .venv/Scripts/python.exe -X utf8 src/otopilot/calculations/wez_grafikler/wez_tarama.py --grup atan_hizi
Her aspect kendi dosyasına yazar; üç terminalde aynı anda koşturulabilir.

Tek yönlülük kontrolü (sınırın ötesinde tekrar erişim var mı), birkaç senaryoda kaba tarama:
  .venv/Scripts/python.exe -X utf8 src/otopilot/calculations/wez_grafikler/wez_tarama.py --kontrol
"""
import argparse
import contextlib
import csv
import io
import math
import sys
import time
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parents[2] / "sim"))   # src/sim
from scenario import WORKSPACE, load_scenario       # noqa: E402
from simulation import Simulation                   # noqa: E402

MPS_TO_KT = 1.94384
NM_TO_M = 1852.0

ALTITUDES_M = (4000, 6000, 8000, 10000)
TARGET_SPEEDS_MPS = (220, 300, 380)
TARGET_THROTTLES = (0.3, 0.6, 1.0)
SHOOTER_SPEEDS_MPS = (220, 300, 380)
SHOOTER_SPEED_MPS = 300
SHOOTER_THROTTLE = 0.45
HEADINGS = {"hot": 0, "beam": 90, "cold": 180}   # blue'nun yönü; red hep 180

RANGE_MIN_NM, RANGE_MAX_NM, PRECISION_NM = 1.0, 150.0, 0.15
M_PER_DEG_LAT = NM_TO_M * 60.0
VERSION = "v2"

FIELDS = ["grup", "aspect", "alt_m", "hedef_hiz_mps", "hedef_gaz", "atan_hiz_mps",
          "raero_nm", "durum", "en_yakin_m", "isabet",
          "fuze_kt_en_yakin", "fuze_mach_en_yakin", "hedef_kt_en_yakin",
          "hedef_ort_kt", "hedef_atana_dogru_ort_kt", "ucus_s", "kosu", "gercek_s"]


def toward_speed_kt(target, shooter):
    """Hedefin hızının atana doğru bileşeni (kt); + atana geliyor. Kısa mesafede düz dünya yaklaşımı."""
    lat_t = float(target.fdm["position/lat-geod-deg"])
    lat_s = float(shooter.fdm["position/lat-geod-deg"])
    d_north = lat_s - lat_t
    d_east = (shooter.get_long_gc_deg() - target.get_long_gc_deg()) * math.cos(math.radians(lat_t))
    norm = math.hypot(d_north, d_east)
    if norm == 0.0:
        return 0.0
    v_toward = (target.get_v_north() * d_north + target.get_v_east() * d_east) / norm
    return v_toward * MPS_TO_KT


def target_ahead_m(missile, target):
    """Hedefin füzenin gidiş yönündeki uzaklığı (m): (hedef − füze) · füze hızının yönü.
    0 ya da eksi ise hedef füzenin yanında ya da arkasında: füze geçti. Kısa mesafede düz dünya yaklaşımı."""
    lat_m = float(missile.fdm["position/lat-geod-deg"])
    lat_t = float(target.fdm["position/lat-geod-deg"])
    d_north = (lat_t - lat_m) * M_PER_DEG_LAT
    d_east = (target.get_long_gc_deg() - missile.get_long_gc_deg()) * M_PER_DEG_LAT * math.cos(math.radians(lat_m))
    d_down = missile.get_altitude() - target.get_altitude()   # hedef alçaktaysa +
    v_north, v_east, v_down = missile.get_v_north(), missile.get_v_east(), missile.get_v_down()
    speed = math.sqrt(v_north * v_north + v_east * v_east + v_down * v_down)
    if speed == 0.0:
        return float("inf")
    return (d_north * v_north + d_east * v_east + d_down * v_down) / speed


def shoot(alt_m, range_nm, aspect, target_speed, target_throttle, shooter_speed):
    """Tek atış. Erişti mi ve en yakın geçişteki değerler."""
    sc = load_scenario()
    sc["world"]["duration_s"] = 900
    blue, red = sc["agents"]["blue"], sc["agents"]["red"]
    for agent in (blue, red):
        agent["spawn"]["alt"] = alt_m
        agent["spawn"]["long"] = 18.0
    blue["spawn"]["heading"] = HEADINGS[aspect]
    red["spawn"]["heading"] = 180
    red["spawn"]["lat"] = blue["spawn"]["lat"] + range_nm / 60.0
    blue["spawn"]["vel"], blue["throttle"] = target_speed, target_throttle
    red["spawn"]["vel"], red["throttle"] = shooter_speed, SHOOTER_THROTTLE

    with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
        sim = Simulation(sc, WORKSPACE / "BVRGym")
        cmds = sim.read_flight_commands()
        sim.step(cmds)
        sim.fire("red", "blue")
        missile = next(m for m in sim.agents["red"].ammo.values() if not m.is_ready_to_launch())
        target, shooter = sim.agents["blue"].simObj, sim.agents["red"].simObj

        d_min = float("inf")
        at_min = {}
        target_kt, toward_kt = [], []
        reached = False
        while missile.is_active() and not sim.finished():
            sim.step(cmds)
            target_kt.append(target.get_true_airspeed() * MPS_TO_KT)
            toward_kt.append(toward_speed_kt(target, shooter))
            if missile.target_hit:
                reached = True
                break
            d = missile.target_norm
            if d < d_min:
                d_min = d
                at_min = dict(fuze_kt_en_yakin=missile.get_true_airspeed() * MPS_TO_KT,
                              fuze_mach_en_yakin=missile.get_mach(),
                              hedef_kt_en_yakin=target.get_true_airspeed() * MPS_TO_KT,
                              ucus_s=missile.get_sim_time_sec())
            if missile.is_active() and target_ahead_m(missile, target) <= 0.0:
                reached = True   # hâlâ uçarken hedefi geçti
                break

    result = dict(erisim=reached, isabet=bool(missile.target_hit), en_yakin_m=d_min,
                  hedef_ort_kt=sum(target_kt) / max(len(target_kt), 1),
                  hedef_atana_dogru_ort_kt=sum(toward_kt) / max(len(toward_kt), 1))
    result.update(at_min)
    return result


def raero(alt_m, aspect, target_speed, target_throttle, shooter_speed):
    """İkiye bölme: erişilen en uzak atış menzili (NM) ve o atışın değerleri."""
    runs = 0

    def try_at(rng):
        nonlocal runs
        runs += 1
        return shoot(alt_m, rng, aspect, target_speed, target_throttle, shooter_speed)

    far = try_at(RANGE_MAX_NM)
    if far["erisim"]:
        return RANGE_MAX_NM, f">{RANGE_MAX_NM:g}", far, runs
    near = try_at(RANGE_MIN_NM)
    if not near["erisim"]:
        return RANGE_MIN_NM, f"<{RANGE_MIN_NM:g}", near, runs

    lo, hi, best = RANGE_MIN_NM, RANGE_MAX_NM, near
    while hi - lo > PRECISION_NM:
        mid = (lo + hi) / 2.0
        res = try_at(mid)
        if res["erisim"]:
            lo, best = mid, res
        else:
            hi = mid
    return lo, "ok", best, runs


def scenarios(group, aspects):
    if group == "ana":
        for aspect in aspects:
            for alt in ALTITUDES_M:
                for speed in TARGET_SPEEDS_MPS:
                    for throttle in TARGET_THROTTLES:
                        yield dict(grup="ana", aspect=aspect, alt_m=alt, hedef_hiz_mps=speed,
                                   hedef_gaz=throttle, atan_hiz_mps=SHOOTER_SPEED_MPS)
    else:
        for aspect in aspects:
            for shooter_speed in SHOOTER_SPEEDS_MPS:
                yield dict(grup="atan_hizi", aspect=aspect, alt_m=8000, hedef_hiz_mps=300,
                           hedef_gaz=0.6, atan_hiz_mps=shooter_speed)


def key_of(row):
    return (row["grup"], row["aspect"], int(float(row["alt_m"])), int(float(row["hedef_hiz_mps"])),
            float(row["hedef_gaz"]), int(float(row["atan_hiz_mps"])))


def sweep(group, aspects):
    name = f"wez_olcumler_{group}_{'_'.join(aspects)}_{VERSION}.csv"
    path = HERE / name
    done = set()
    if path.exists():
        with path.open(encoding="utf-8") as f:
            done = {key_of(row) for row in csv.DictReader(f)}
    new_file = not path.exists()

    with path.open("a", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=FIELDS)
        if new_file:
            writer.writeheader()
        todo = [s for s in scenarios(group, aspects) if key_of(s) not in done]
        print(f"{path.name}: {len(todo)} senaryo kaldı ({len(done)} bitmiş)", flush=True)
        for i, s in enumerate(todo, 1):
            start = time.time()
            rng, status, res, runs = raero(s["alt_m"], s["aspect"], s["hedef_hiz_mps"],
                                           s["hedef_gaz"], s["atan_hiz_mps"])
            row = dict(s, raero_nm=round(rng, 2), durum=status, en_yakin_m=round(res["en_yakin_m"]),
                       isabet=res["isabet"], kosu=runs, gercek_s=round(time.time() - start, 1),
                       hedef_ort_kt=round(res["hedef_ort_kt"]),
                       hedef_atana_dogru_ort_kt=round(res["hedef_atana_dogru_ort_kt"]))
            for k in ("fuze_kt_en_yakin", "fuze_mach_en_yakin", "hedef_kt_en_yakin", "ucus_s"):
                row[k] = round(res[k], 2) if k in res else ""
            writer.writerow(row)
            f.flush()
            print(f"[{i}/{len(todo)}] {s['aspect']:4} {s['alt_m']:5} m  hedef {s['hedef_hiz_mps']} m/s gaz {s['hedef_gaz']}"
                  f"  atan {s['atan_hiz_mps']} m/s  ->  Raero {rng:5.2f} NM ({status})"
                  f"  en yakın {res['en_yakin_m']:6.0f} m  {runs} koşu  {row['gercek_s']} s", flush=True)


def check():
    """Kaba tarama: 4-100 NM, 4 NM adım. Erişim deseni tek yönlü mü (E...E-...-)?"""
    path = HERE / f"wez_kontrol_{VERSION}.csv"
    cases = [("hot", 4000), ("hot", 10000), ("beam", 6000), ("cold", 8000)]
    with path.open("w", newline="", encoding="utf-8") as f:
        writer = csv.writer(f)
        writer.writerow(["aspect", "alt_m", "menzil_nm", "erisim", "isabet", "en_yakin_m", "fuze_mach_en_yakin"])
        for aspect, alt in cases:
            pattern = ""
            for rng in range(4, 104, 4):
                res = shoot(alt, rng, aspect, 300, 0.6, SHOOTER_SPEED_MPS)
                writer.writerow([aspect, alt, rng, res["erisim"], res["isabet"], round(res["en_yakin_m"]),
                                 round(res.get("fuze_mach_en_yakin", 0), 2)])
                f.flush()
                pattern += "E" if res["erisim"] else "-"
            monotone = "-E" not in pattern
            print(f"{aspect:4} {alt:5} m  4..100 NM: {pattern}  {'tek yönlü' if monotone else 'TEK YÖNLÜ DEĞİL'}", flush=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--aspect", nargs="+", choices=sorted(HEADINGS), default=["hot", "beam", "cold"])
    parser.add_argument("--grup", choices=["ana", "atan_hizi"], default="ana")
    parser.add_argument("--kontrol", action="store_true", help="tek yönlülük kontrolü (kaba tarama)")
    args = parser.parse_args()

    if args.kontrol:
        check()
    elif args.grup == "atan_hizi":
        sweep("atan_hizi", [a for a in args.aspect if a != "beam"] or ["hot", "cold"])
    else:
        sweep("ana", args.aspect)
