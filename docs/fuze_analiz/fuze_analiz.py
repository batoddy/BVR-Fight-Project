"""mar_test kayıtlarından hız (m/s) eksenli süzülme tabloları çıkarır ve kaydırma hipotezini test eder.

Hipotez: aynı irtifada füze, atış hızı ne olursa olsun aynı yavaşlama eğrisini izler; daha hızlı atılan
füze eğriye daha yukarıdan girer. Tepe hız = uçağın atış hızı + Δv(irtifa).
Atıcı hızlıyken füze hedefi vurursa eğri yarıda kalır; o irtifanın tablosu bir sonraki kayıtla birleştirilir.

Kayıtlar: python src/sim/sim_main.py --name mar_test_<N>km_6g_new_otopilot_<thr>_throttle --altitude <m> --throttle <0..1>
Kullanım (Workspace klasöründen):
  .venv\\Scripts\\python.exe docs\\fuze_analiz\\fuze_analiz.py recordings > docs\\fuze_analiz\\sonuclar.txt
"""
import sys, glob, os, re, json
import numpy as np
import pandas as pd

sys.stdout.reconfigure(encoding="utf-8")   # Windows konsolu (cp1254) Δ basamıyor
KT2MS = 0.514444
MACH_MARKS = (2.0, 1.5, 1.2, 1.0)
GRID = np.arange(1400, 249, -25)      # m/s, tablo satırları (yüksekten alçağa)
CHECK = np.arange(900, 349, -50)      # m/s, hipotez kontrol noktaları


def label(d):
    name = os.path.basename(d)
    alt = int(re.search(r"mar_test_(\d+)km", name).group(1))
    if alt < 100:   # "6km" gibi km ile yazılmış klasör adları
        alt *= 1000
    return alt, name


def load_run(d):
    ev = [json.loads(l) for l in open(os.path.join(d, "events.jsonl"), encoding="utf-8")]
    launch = next(e for e in ev if e["type"] == "missile_launched" and e["agent"] == "red")
    t_launch = launch["sim_time_s"]

    ac = pd.read_csv(glob.glob(os.path.join(d, "F-16 (Hostile*"))[0])
    row = ac.iloc[(ac["time_s"] - t_launch).abs().idxmin()]
    v_launch = row["tas_kt"] * KT2MS
    throttle = row.get("cmd_throttle_norm", np.nan)

    m = pd.read_csv(glob.glob(os.path.join(d, "AIM-120*red--0*"))[0]).dropna(subset=["tas_kt"])
    m = m.sort_values("missile_time_s")
    t = m["missile_time_s"].to_numpy()
    v = m["tas_kt"].to_numpy() * KT2MS
    mach = m["mach"].to_numpy()
    dist = np.concatenate([[0], np.cumsum(0.5 * (v[1:] + v[:-1]) * np.diff(t))])
    k = int(np.argmax(v))
    end = [e["type"] for e in ev if e["agent"] == "red" and e["type"] in ("missile_hit", "missile_lost")]
    marks = {}
    for M in MACH_MARKS:   # tepe hızdan sonra Mach'ın ilk kez M'nin altına indiği an
        idx = np.where(mach[k:] < M)[0]
        if len(idx):
            j = k + idx[0]
            marks[M] = (t[j], dist[j], m["alt_ft"].iloc[j] * 0.3048)
    return dict(v_launch=v_launch, throttle=throttle, v_peak=v[k], t_peak=t[k], d_peak=dist[k],
                v=v[k:], t=t[k:], d=dist[k:], alt0=m["alt_ft"].iloc[0] * 0.3048,
                marks=marks, t_end=t[-1], d_end=dist[-1], end=end[0] if end else "sim bitti")


def first_cross(r, V):
    """Tepe hızdan sonra hızın ilk kez V'ye indiği an: (yol m, süre s), doğrusal ara değerle."""
    idx = np.where(r["v"] <= V)[0]
    if not len(idx) or idx[0] == 0:
        return None
    i = idx[0]
    f = (r["v"][i - 1] - V) / (r["v"][i - 1] - r["v"][i])
    return (r["d"][i - 1] + f * (r["d"][i] - r["d"][i - 1]), r["t"][i - 1] + f * (r["t"][i] - r["t"][i - 1]))


runs = {}
for d in sorted(glob.glob(os.path.join(sys.argv[1], "mar_test_*_new_otopilot*"))):
    alt, name = label(d)
    try:
        runs[name] = (alt, load_run(d))
    except Exception as e:
        print(f"ATLANDI {name}: {e}")

print("=== Atış ve tepe hızlar ===")
print(f"{'kayıt':45} {'irtifa':>6} {'thr':>5} {'atış m/s':>9} {'tepe m/s':>9} {'Δv':>6} {'boost yolu m':>12}")
for name, (alt, r) in sorted(runs.items(), key=lambda x: (x[1][0], x[1][1]["v_launch"])):
    print(f"{name:45} {alt:6d} {r['throttle']:5.2f} {r['v_launch']:9.1f} {r['v_peak']:9.1f} "
          f"{r['v_peak'] - r['v_launch']:6.1f} {r['d_peak']:12.0f}")

print("\n=== Mach eşiklerinin altına iniş: süre s / yol km (atıştan itibaren) / füze irtifası km ===")
print(f"{'irtifa':>6} {'atış m/s':>8} " + "".join(f"{'M<' + str(M):>20}" for M in MACH_MARKS) + f"{'bitiş':>24}")
for name, (alt, r) in sorted(runs.items(), key=lambda x: (x[1][0], x[1][1]["v_launch"])):
    line = f"{alt:6d} {r['v_launch']:8.0f} "
    for M in MACH_MARKS:
        c = r["marks"].get(M)
        line += f"{c[0]:7.1f} {c[1]/1e3:6.1f} {c[2]/1e3:5.1f}" if c else f"{'-':>20}"
    line += f"   {r['t_end']:6.1f} s {r['d_end']/1e3:5.1f} km {r['end']}"
    print(line)

# Manevralı, düz süzülme olmayan kayıtlar: tablo ve kontrol dışı.
EXCLUDE = {"mar_test_10000km_6g_new_otopilot_070mah"}
by_alt = {}
for name, (alt, r) in runs.items():
    if name not in EXCLUDE:
        r["name"] = name
        by_alt.setdefault(alt, []).append(r)


def composite(lst):
    """En hızlı kayıttan başla; bittiği yerde bir sonrakini aynı hızda hizalayıp devam et.
    Segment: [v_üst, v_alt, kayıt, yol ofseti, süre ofseti]. Yol/süre birleşik eğrinin tepesinden."""
    lst = sorted(lst, key=lambda r: -r["v_peak"])
    r0 = lst[0]
    segs = [[r0["v_peak"], r0["v"].min(), r0, -r0["d"][0], -r0["t"][0]]]
    for r in lst[1:]:
        lo = segs[-1][1]
        vj = lo + 25.0
        if r["v"].min() >= lo - 1 or r["v_peak"] <= vj:
            continue   # bu kayıt eğriyi aşağı uzatmıyor
        a, b = eval_comp(segs, vj), first_cross(r, vj)
        segs[-1][1] = vj
        segs.append([vj, r["v"].min(), r, a[0] - b[0], a[1] - b[1]])
    return segs


def eval_comp(segs, V):
    for hi, lo, r, od, ot in segs:
        if lo <= V <= hi:
            c = first_cross(r, V)
            return None if c is None else (c[0] + od, c[1] + ot)
    return None


print("\n=== Kaydırma kontrolü (leave-one-out): her kayıt, kendisi hariç birleşik eğriden tahmin ===")
print("hata % (yol / süre), atıştan sonraki tepe hızdan V'ye kadar")
CK = (1000, 800, 600, 500, 400, 350, 300)
print(f"{'kayıt':45} {'atış':>5} " + "".join(f"{V:>13}" for V in CK))
for alt, lst in sorted(by_alt.items()):
    for r in sorted(lst, key=lambda r: r["v_launch"]):
        others = [o for o in lst if o is not r]
        if not others:
            continue
        segs = composite(others)
        base = eval_comp(segs, r["v_peak"])
        if base is None:
            print(f"{r['name']:45} {r['v_launch']:5.0f}  (diğerlerinin tepe hızı bu atışınkinden düşük)")
            continue
        line = f"{r['name']:45} {r['v_launch']:5.0f} "
        for V in CK:
            a, b = first_cross(r, V), eval_comp(segs, V)
            if a is None or b is None:
                line += f"{'-':>13}"
                continue
            dm, tm = a[0] - r["d"][0], a[1] - r["t"][0]
            line += f"{100*((b[0]-base[0])-dm)/dm:6.1f}/{100*((b[1]-base[1])-tm)/tm:5.1f} "
        print(line)

print("\n=== Δv(h) fit'i ===")
H = np.array([alt / 1e3 for alt, lst in by_alt.items() for r in lst])
VL = np.array([r["v_launch"] for alt, lst in by_alt.items() for r in lst])
DV = np.array([r["v_peak"] - r["v_launch"] for alt, lst in by_alt.items() for r in lst])
c2 = np.polyfit(H - 10, DV, 2)
res = DV - np.polyval(c2, H - 10)
print(f"Δv = {c2[2]:.1f} + {c2[1]:.2f}·(h−10) + {c2[0]:.3f}·(h−10)²   en büyük artık {np.abs(res).max():.1f} m/s")
A = np.vstack([np.ones_like(H), H - 10, (H - 10) ** 2, VL - 400]).T
c4, *_ = np.linalg.lstsq(A, DV, rcond=None)
res4 = DV - A @ c4
print(f"Δv = {c4[0]:.1f} + {c4[1]:.2f}·(h−10) + {c4[2]:.3f}·(h−10)² + {c4[3]:.4f}·(V_atış−400)   "
      f"en büyük artık {np.abs(res4).max():.1f} m/s")

print("\n=== Birleşik hız eksenli tablolar (yol m / süre s, eğrinin tepesinden itibaren) ===")
for alt, lst in sorted(by_alt.items()):
    segs = composite(lst)
    src = " + ".join(f"{s[2]['name']} ({s[0]:.0f}→{s[1]:.0f} m/s)" for s in segs)
    print(f"// {alt} m, tepe {segs[0][0]:.0f} m/s. Kaynak: {src}")
    for V in GRID:
        c = eval_comp(segs, V)
        if c is not None:
            print(f"    {{ {V:5.0f}, {c[0]:8.0f}, {c[1]:6.1f} }},")
