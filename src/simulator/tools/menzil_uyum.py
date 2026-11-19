"""menzil_tarama.py çıktılarının uyumu: Rmax (hot), R_cold ve füze hız modeli.

Workspace içinden: .venv/Scripts/python.exe -X utf8 src/simulator/tools/menzil_uyum.py
Girdi/çıktı: src/otopilot/olcumler/faz4_olcum/ (menzil_olcumler.csv, hiz_zaman.csv -> uyum_sonuc.txt)
Yöntem eski uyumla aynı (wez_grafik.py): ln h ve ln R tabanlı en küçük kareler; Rmax için k ayrıca taranır.
"""
import csv
from pathlib import Path

import numpy as np

OUT = Path(__file__).resolve().parents[2] / "otopilot" / "olcumler" / "faz4_olcum"
KT = 1852.0 / 3600.0
BURN_END_S = 7.75           # yanma sonu (aim120c5.yaml burn_s)
LINES = []


def say(text=""):
    print(text)
    LINES.append(text)


def load(name):
    with (OUT / name).open(newline="", encoding="utf-8") as file:
        return list(csv.DictReader(file))


def fit_hot(rows):
    """Rmax = (a + b·ln h)·(1 + k·(Vs − 583)) + V·(c + d·ln h)/3600; k taranır, a,b,c,d doğrusal."""
    ln_h = np.array([np.log(r["altitude_m"] / 1000) for r in rows])
    v = np.array([r["target_speed_mps"] / KT for r in rows])
    shooter = np.array([r["shooter_speed_mps"] / KT - 583.0 for r in rows])
    y = np.array([r["range_nm"] for r in rows])

    def solve(k):
        scale = 1 + k * shooter
        design = np.column_stack([scale, scale * ln_h, v / 3600, v * ln_h / 3600])
        coef, *_ = np.linalg.lstsq(design, y, rcond=None)
        return coef, float(np.sum((design @ coef - y) ** 2)), design @ coef

    grid = np.linspace(-0.003, 0.003, 601)
    k = min(grid, key=lambda value: solve(value)[1])
    return solve(k)[0], k, solve(k)[2]


def fit_cold(rows):
    x = np.array([r["altitude_m"] / 1000 - 10 for r in rows])
    y = np.array([r["target_speed_mps"] / KT - 700 for r in rows])
    ln_r = np.log([r["range_nm"] for r in rows])
    coef, *_ = np.linalg.lstsq(np.column_stack([np.ones_like(x), x, y, x * y]), ln_r, rcond=None)
    return coef, np.exp(coef[0] + coef[1] * x + coef[2] * y + coef[3] * x * y)


def report_points(rows, model):
    for r, m in zip(rows, model):
        say(f"  {int(r['altitude_m']):5d} m {r['aspect']:4} hedef {int(r['target_speed_mps'])} m/s atan {int(r['shooter_speed_mps'])} m/s: "
            f"ölçülen {r['range_nm']:6.2f} NM  model {m:6.2f} NM  hata {m - r['range_nm']:+6.2f} NM ({100 * (m / r['range_nm'] - 1):+5.1f} %)")


def linear_fit(t, v):
    design = np.column_stack([np.ones_like(t), t])
    (v0, slope), *_ = np.linalg.lstsq(design, v, rcond=None)
    error = design @ np.array([v0, slope]) - v
    return v0, slope, float(np.sqrt(np.mean(error ** 2))), float(np.abs(error).max())


def main():
    rows = [r for r in load("menzil_olcumler.csv") if r["range_nm"] != ""]
    for r in rows:
        for key in ("altitude_m", "target_speed_mps", "shooter_speed_mps", "range_nm"):
            r[key] = float(r[key])
    skipped = [r for r in load("menzil_olcumler.csv") if r["range_nm"] == ""]
    if skipped:
        say(f"1 NM'de bile erişilemeyen senaryo: {len(skipped)} ({[(r['altitude_m'], r['aspect'], r['target_speed_mps'], r['shooter_speed_mps']) for r in skipped]})")

    hot = [r for r in rows if r["aspect"] == "hot"]
    coef, k, model = fit_hot(hot)
    a, b, c, d = coef
    say(f"=== Rmax (hot, {len(hot)} nokta): (a + b·ln h)·(1 + k·(Vatan_kt − 583)) + V·(c + d·ln h)/3600 ===")
    say(f"a = {a:.3f}  b = {b:.3f}  c = {c:.2f}  d = {d:.2f}  k = {k:.6f}   (eski: a -29.595 b 39.193 c -48.62 d 95.66 k 0.000417)")
    report_points(hot, model)
    err = model - np.array([r["range_nm"] for r in hot])
    say(f"  ortalama |hata| {np.abs(err).mean():.2f} NM, en büyük {err.max():+.2f} / {err.min():+.2f} NM")
    say()

    cold = [r for r in rows if r["aspect"] == "cold" and r["shooter_speed_mps"] == 300]
    coef, model = fit_cold(cold)
    say(f"=== R_cold (cold, atan 300 m/s, {len(cold)} nokta): exp(a + b·x + c·y + d·x·y), x = h_km − 10, y = V_kt − 700 ===")
    say(f"a = {coef[0]:.3f}  b = {coef[1]:.4f}  c = {coef[2]:.5f}  d = {coef[3]:.5f}   (eski: 2.646 0.1523 -0.00201 -0.00008)")
    report_points(cold, model)
    err = model - np.array([r["range_nm"] for r in cold])
    ratio = np.array([r["range_nm"] for r in cold]) / model
    say(f"  ortalama |hata| {np.abs(err).mean():.2f} NM, ölçülen/model {ratio.min():.3f}–{ratio.max():.3f}")
    say()

    speed = [(float(r["altitude_m"]), float(r["time_s"]), float(r["missile_speed_kt"])) for r in load("hiz_zaman.csv")]
    say(f"=== Hız modeli: v = v0 + a·t, t ≥ {BURN_END_S} s (yanma sonrası); C++ şimdiki: v0 2700 kt, a −18 kt/s ===")
    pooled_t, pooled_v = [], []
    for alt in sorted({s[0] for s in speed}):
        pts = [(t, v) for h, t, v in speed if h == alt and t >= BURN_END_S]
        t, v = np.array([p[0] for p in pts]), np.array([p[1] for p in pts])
        pooled_t.append(t)
        pooled_v.append(v)
        v0, slope, rmse, worst = linear_fit(t, v)
        say(f"  {int(alt):5d} m: v0 = {v0:7.1f} kt  a = {slope:6.2f} kt/s  rms {rmse:5.1f} kt  en büyük {worst:5.1f} kt  (t {t[0]:.1f}–{t[-1]:.1f} s, v {v[0]:.0f}→{v[-1]:.0f} kt)")
    v0, slope, rmse, worst = linear_fit(np.concatenate(pooled_t), np.concatenate(pooled_v))
    say(f"  hepsi : v0 = {v0:7.1f} kt  a = {slope:6.2f} kt/s  rms {rmse:5.1f} kt  en büyük {worst:5.1f} kt")

    (OUT / "uyum_sonuc.txt").write_text("\n".join(LINES) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
