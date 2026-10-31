"""WEZ (Raero) formülünün uyumu ve grafikleri; R_cold'un yeni (erişim) ölçümle karşılaştırması.

Workspace içinden: .venv/Scripts/python.exe -X utf8 src/otopilot/calculations/wez_grafikler/wez_grafik.py
Girdi : aynı klasördeki *_v2.csv (wez_tarama.py çıktıları)
Çıktı : ekrana katsayılar ve hatalar; wez_sonuc.txt (aynısı); wez_model.png, wez_karsilastirma.png,
        r_cold_eski_yeni.png, wez_beam.png

Hot model (düşman bana geliyor):
  Raero = R_m + V · t / 3600
    R_m = a + b·ln(h)     füzenin kendi gittiği yol (NM), h km
    t   = c + d·ln(h)     uçuş süresi (s)
    V   = düşmanın bana doğru ortalama hızı (kt)
  Dört katsayı tek seferde, Raero'nun kendisine en küçük karelerle uydurulur (model katsayılarda doğrusal).
Atan hızı: 8 km'deki üç ölçümden R_m'nin atan hızına göre oranı: R_m · (1 + k·(V_atan − 300 m/s)).
R_cold: eski formül (r_cold_formulu.md) yeni erişim ölçümüne karşı; aynı biçimde yeniden uyum da verilir.
"""
import csv
from pathlib import Path

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np

HERE = Path(__file__).resolve().parent
OUT_LINES = []

# Eski R_cold formülü (r_cold_formulu.md; koddaki AIM120C_R_COLD): exp(a + b·x + c·y + d·x·y)
OLD_A, OLD_B, OLD_C, OLD_D = 2.646, 0.1523, -0.00201, -0.00008
R_COLD_FIT_MIN_KT = 550

ALT_COLORS = {4000: "#86b6ef", 6000: "#3987e5", 8000: "#256abf", 10000: "#0d366b"}
INK, INK_MUTED, GRID, SURFACE = "#1f1f1e", "#6b6a64", "#e4e3de", "#fcfcfb"


def say(text=""):
    print(text)
    OUT_LINES.append(text)


def load(name):
    with open(HERE / name, newline="", encoding="utf-8") as file:
        rows = list(csv.DictReader(file))
    numeric = ("alt_m", "hedef_hiz_mps", "hedef_gaz", "atan_hiz_mps", "raero_nm", "hedef_ort_kt",
               "hedef_atana_dogru_ort_kt", "ucus_s")
    for row in rows:
        for key in numeric:
            row[key] = float(row[key])
    return [r for r in rows if r["durum"] == "ok"]


def style(ax, title, x_label, y_label):
    ax.set_facecolor(SURFACE)
    ax.set_title(title, loc="left", color=INK, fontsize=13, fontweight="bold")
    ax.set_xlabel(x_label, color=INK_MUTED)
    ax.set_ylabel(y_label, color=INK_MUTED)
    ax.grid(axis="y", color=GRID, linewidth=1)
    ax.set_axisbelow(True)
    for side in ("top", "right", "left"):
        ax.spines[side].set_visible(False)
    ax.spines["bottom"].set_color(INK_MUTED)
    ax.tick_params(colors=INK_MUTED, length=0)


# ---------------------------------------------------------------- hot
def fit_hot(rows):
    """Raero = a + b·ln(h) + V·(c + d·ln(h))/3600 → a, b, c, d."""
    ln_h = np.array([np.log(r["alt_m"] / 1000) for r in rows])
    v = np.array([r["hedef_atana_dogru_ort_kt"] for r in rows])
    y = np.array([r["raero_nm"] for r in rows])
    design = np.column_stack([np.ones_like(ln_h), ln_h, v / 3600, v * ln_h / 3600])
    coef, *_ = np.linalg.lstsq(design, y, rcond=None)
    return coef


def hot_model(coef, alt_m, v_kt):
    a, b, c, d = coef
    ln_h = np.log(alt_m / 1000)
    return (a + b * ln_h) + v_kt * (c + d * ln_h) / 3600


def report_hot(rows, coef):
    a, b, c, d = coef
    say("=== Hot: Raero = (a + b·ln h) + V·(c + d·ln h)/3600   [h km, V kt, Raero NM] ===")
    say(f"a = {a:.3f}   b = {b:.3f}   (füzenin kendi yolu R_m, NM)")
    say(f"c = {c:.2f}   d = {d:.2f}   (uçuş süresi t, s)")
    for alt in (4000, 6000, 8000, 10000):
        ln_h = np.log(alt / 1000)
        say(f"  {alt / 1000:4g} km:  R_m = {a + b * ln_h:6.2f} NM   t = {c + d * ln_h:6.1f} s")
    errors = np.array([hot_model(coef, r["alt_m"], r["hedef_atana_dogru_ort_kt"]) - r["raero_nm"] for r in rows])
    ratios = np.array([r["raero_nm"] / hot_model(coef, r["alt_m"], r["hedef_atana_dogru_ort_kt"]) for r in rows])
    say(f"Hata (model − ölçülen): ortalama |hata| {np.mean(np.abs(errors)):.2f} NM, "
        f"en büyük fazla tahmin {errors.max():.2f} NM, en büyük eksik tahmin {errors.min():.2f} NM")
    say(f"Ölçülen / model oranı: {ratios.min():.3f} – {ratios.max():.3f}")
    say(f"  -> model × {np.floor(ratios.min() * 100) / 100:.2f} ile bütün noktalarda ölçülenin altında kalır (fazla tahmin yok)")
    say()


def report_shooter(rows_atan, coef):
    say("=== Atan hızı (8 km, hedef 300 m/s gaz 0.6) ===")
    for aspect in ("hot", "cold"):
        pts = sorted([r for r in rows_atan if r["aspect"] == aspect], key=lambda r: r["atan_hiz_mps"])
        if not pts:
            continue
        base = next(r for r in pts if r["atan_hiz_mps"] == 300)
        if aspect == "hot":
            def own_path(r):
                return r["raero_nm"] - r["hedef_atana_dogru_ort_kt"] * r["ucus_s"] / 3600
        else:
            def own_path(r):
                return r["raero_nm"]
        base_value = own_path(base)
        slopes = []
        for r in pts:
            ratio = own_path(r) / base_value
            if r["atan_hiz_mps"] != 300:
                slopes.append((ratio - 1) / (r["atan_hiz_mps"] - 300))
            say(f"  {aspect:4} atan {r['atan_hiz_mps']:.0f} m/s: Raero {r['raero_nm']:6.2f} NM"
                f"   {'füzenin kendi yolu' if aspect == 'hot' else 'Raero'} oranı {ratio:.3f}")
        say(f"  {aspect}: k = {np.mean(slopes):.5f} /(m/s)   ->  × (1 + k·(V_atan − 300 m/s))")
    say()


def chart_hot(rows, coef):
    fig, ax = plt.subplots(figsize=(9, 5.6), facecolor=SURFACE)
    style(ax, "Raero (hot): ölçülen ve model", "Düşmanın bana doğru ortalama hızı (kt)", "Raero (NM)")
    speeds = np.arange(380, 1180, 5)
    for alt, color in ALT_COLORS.items():
        ax.plot(speeds, hot_model(coef, alt, speeds), color=color, linewidth=2)
        pts = [r for r in rows if r["alt_m"] == alt]
        ax.scatter([r["hedef_atana_dogru_ort_kt"] for r in pts], [r["raero_nm"] for r in pts],
                   s=42, color=color, edgecolors=SURFACE, linewidths=1.5, zorder=3)
        ax.annotate(f"{alt / 1000:g} km", (speeds[-1], hot_model(coef, alt, speeds[-1])), xytext=(8, 0),
                    textcoords="offset points", va="center", color=INK, fontsize=10)
    ax.legend(handles=[plt.Line2D([], [], color=INK_MUTED, linewidth=2, label="model"),
                       plt.Line2D([], [], color=INK_MUTED, marker="o", linestyle="", markersize=7, label="ölçülen")],
              frameon=False, labelcolor=INK_MUTED, loc="upper left", fontsize=9)
    ax.set_xlim(360, 1260)
    ax.set_ylim(0, 125)
    fig.tight_layout()
    fig.savefig(HERE / "wez_model.png", dpi=150)
    plt.close(fig)

    fig, ax = plt.subplots(figsize=(6.2, 5.8), facecolor=SURFACE)
    style(ax, "Raero: ölçülen ve model", "Model Raero (NM)", "Ölçülen Raero (NM)")
    limit = 120
    ax.plot([0, limit], [0, limit], color=INK_MUTED, linewidth=1.5, label="ölçülen = model")
    for alt, color in ALT_COLORS.items():
        pts = [r for r in rows if r["alt_m"] == alt]
        ax.scatter([hot_model(coef, alt, r["hedef_atana_dogru_ort_kt"]) for r in pts], [r["raero_nm"] for r in pts],
                   s=42, color=color, edgecolors=SURFACE, linewidths=1.5, zorder=3, label=f"{alt / 1000:g} km")
    ax.legend(frameon=False, labelcolor=INK_MUTED, loc="upper left", fontsize=9)
    ax.set_xlim(0, limit)
    ax.set_ylim(0, limit)
    ax.set_aspect("equal")
    fig.tight_layout()
    fig.savefig(HERE / "wez_karsilastirma.png", dpi=150)
    plt.close(fig)


# ---------------------------------------------------------------- R_cold
def r_cold_model(coef, alt_m, v_kt):
    a, b, c, d = coef
    x, y = alt_m / 1000 - 10, v_kt - 700
    return np.exp(a + b * x + c * y + d * x * y)


def report_r_cold(rows):
    """Eski formül yeni ölçüme karşı; aynı biçimde (ln R = a + b·x + c·y + d·x·y) yeniden uyum."""
    old = (OLD_A, OLD_B, OLD_C, OLD_D)
    fitted = [r for r in rows if r["hedef_ort_kt"] >= R_COLD_FIT_MIN_KT]
    x = np.array([r["alt_m"] / 1000 - 10 for r in fitted])
    y = np.array([r["hedef_ort_kt"] - 700 for r in fitted])
    ln_r = np.log([r["raero_nm"] for r in fitted])
    new, *_ = np.linalg.lstsq(np.column_stack([np.ones_like(x), x, y, x * y]), ln_r, rcond=None)

    say(f"=== R_cold: ln R = a + b·x + c·y + d·x·y  [x = h − 10 km, y = V_kaçış − 700 kt], {len(fitted)} nokta (V ≥ {R_COLD_FIT_MIN_KT} kt) ===")
    for name, coef in (("eski (koddaki)", old), ("yeni (erişim) ", new)):
        ratios = np.array([r["raero_nm"] / r_cold_model(coef, r["alt_m"], r["hedef_ort_kt"]) for r in fitted])
        errors = np.array([r_cold_model(coef, r["alt_m"], r["hedef_ort_kt"]) - r["raero_nm"] for r in fitted])
        say(f"{name}: a={coef[0]:.3f} b={coef[1]:.4f} c={coef[2]:.5f} d={coef[3]:.5f}   "
            f"ortalama |hata| {np.mean(np.abs(errors)):.2f} NM, ölçülen/model {ratios.min():.3f}–{ratios.max():.3f}"
            f"  -> güvenli çarpan ≥ {np.ceil(ratios.max() * 100) / 100:.2f}")
    say()
    return new


def chart_r_cold(rows, new):
    fig, ax = plt.subplots(figsize=(9, 5.6), facecolor=SURFACE)
    style(ax, "R_cold: eski formül ve yeni erişim ölçümü", "Kaçış hızı, ortalama (kt)", "R_cold (NM)")
    speeds = np.arange(560, 1001, 5)
    old = (OLD_A, OLD_B, OLD_C, OLD_D)
    for alt, color in ALT_COLORS.items():
        ax.plot(speeds, r_cold_model(old, alt, speeds), color=color, linewidth=1.5, linestyle=(0, (5, 4)))
        ax.plot(speeds, r_cold_model(new, alt, speeds), color=color, linewidth=2)
        pts = [r for r in rows if r["alt_m"] == alt]
        ax.scatter([r["hedef_ort_kt"] for r in pts], [r["raero_nm"] for r in pts],
                   s=42, color=color, edgecolors=SURFACE, linewidths=1.5, zorder=3)
        ax.annotate(f"{alt / 1000:g} km", (1000, r_cold_model(new, alt, 1000)), xytext=(8, 0),
                    textcoords="offset points", va="center", color=INK, fontsize=10)
    ax.legend(handles=[plt.Line2D([], [], color=INK_MUTED, linewidth=2, label="yeni uyum"),
                       plt.Line2D([], [], color=INK_MUTED, linewidth=1.5, linestyle=(0, (5, 4)), label="eski formül"),
                       plt.Line2D([], [], color=INK_MUTED, marker="o", linestyle="", markersize=7, label="erişim ölçümü")],
              frameon=False, labelcolor=INK_MUTED, loc="upper right", fontsize=9)
    ax.set_xlim(400, 1060)
    ax.set_ylim(0, 30)
    fig.tight_layout()
    fig.savefig(HERE / "r_cold_eski_yeni.png", dpi=150)
    plt.close(fig)


# ---------------------------------------------------------------- beam
def chart_beam(rows):
    fig, ax = plt.subplots(figsize=(9, 5.6), facecolor=SURFACE)
    style(ax, "Raero (beam): ölçülen", "Hedefin ortalama hızı (kt)", "Raero (NM)")
    for alt, color in ALT_COLORS.items():
        pts = sorted([r for r in rows if r["alt_m"] == alt], key=lambda r: r["hedef_ort_kt"])
        ax.plot([r["hedef_ort_kt"] for r in pts], [r["raero_nm"] for r in pts], color=color, linewidth=1,
                marker="o", markersize=6, label=f"{alt / 1000:g} km")
    ax.legend(frameon=False, labelcolor=INK_MUTED, loc="upper right", fontsize=9)
    fig.tight_layout()
    fig.savefig(HERE / "wez_beam.png", dpi=150)
    plt.close(fig)


if __name__ == "__main__":
    hot = load("wez_olcumler_ana_hot_v2.csv")
    beam = load("wez_olcumler_ana_beam_v2.csv")
    cold = load("wez_olcumler_ana_cold_v2.csv")
    shooter = load("wez_olcumler_atan_hizi_hot_cold_v2.csv")

    hot_coef = fit_hot(hot)
    report_hot(hot, hot_coef)
    report_shooter(shooter, hot_coef)
    r_cold_new = report_r_cold(cold)

    chart_hot(hot, hot_coef)
    chart_r_cold(cold, r_cold_new)
    chart_beam(beam)

    (HERE / "wez_sonuc.txt").write_text("\n".join(OUT_LINES) + "\n", encoding="utf-8")
    print("Yazıldı: wez_sonuc.txt, wez_model.png, wez_karsilastirma.png, r_cold_eski_yeni.png, wez_beam.png")
