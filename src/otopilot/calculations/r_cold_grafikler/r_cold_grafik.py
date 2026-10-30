"""R_cold modelinin grafikleri: ölçülen noktalar ve üstel uyum (r_cold_formulu.md).

Workspace içinden: python src/otopilot/calculations/r_cold_grafikler/r_cold_grafik.py
Girdi: aynı klasördeki r_cold_olcumler.csv. Çıktı: r_cold_model.png, r_cold_karsilastirma.png
"""
import csv
from pathlib import Path

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np

HERE = Path(__file__).resolve().parent

# Formül: R_cold = exp(a + b·x + c·y + d·x·y), x = h − 10 km, y = V − 700 kt
A, B, C, D = 2.646, 0.1523, -0.00201, -0.00008
SAFETY = 1.15
FIT_MIN_KT = 550   # uyum bu hızın üstündeki noktalarla yapıldı

# İrtifa sıralı bir büyüklük: tek tonlu (mavi) sıralı rampa, açıktan koyuya.
ALT_COLORS = {4000: "#86b6ef", 6000: "#3987e5", 8000: "#256abf", 10000: "#0d366b"}
INK, INK_MUTED, GRID, SURFACE = "#1f1f1e", "#6b6a64", "#e4e3de", "#fcfcfb"


def model_nm(alt_m, v_kt):
    x, y = alt_m / 1000 - 10, v_kt - 700
    return np.exp(A + B * x + C * y + D * x * y)


def load():
    with open(HERE / "r_cold_olcumler.csv", newline="") as file:
        return [{k: float(v) for k, v in row.items()} for row in csv.DictReader(file)]


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


def chart_model(rows):
    """R_cold – kaçış hızı: her irtifada ölçülen noktalar, model ve güvenlik eğrisi."""
    fig, ax = plt.subplots(figsize=(9, 5.6), facecolor=SURFACE)
    style(ax, "R_cold modeli: ölçülen sınır ve üstel uyum", "Kaçış hızı, ortalama (kt)", "R_cold (NM)")

    speeds = np.arange(560, 1001, 5)
    for alt, color in ALT_COLORS.items():
        ax.plot(speeds, model_nm(alt, speeds), color=color, linewidth=2)
        ax.plot(speeds, SAFETY * model_nm(alt, speeds), color=color, linewidth=1.5, linestyle=(0, (5, 4)))
        points = [r for r in rows if r["alt_m"] == alt]
        fitted = [r for r in points if r["v_escape_avg_kt"] >= FIT_MIN_KT]
        skipped = [r for r in points if r["v_escape_avg_kt"] < FIT_MIN_KT]
        ax.scatter([r["v_escape_avg_kt"] for r in fitted], [r["r_cold_nm"] for r in fitted],
                   s=42, color=color, edgecolors=SURFACE, linewidths=1.5, zorder=3)
        ax.scatter([r["v_escape_avg_kt"] for r in skipped], [r["r_cold_nm"] for r in skipped],
                   s=42, facecolors=SURFACE, edgecolors=color, linewidths=1.8, zorder=3)
        # Doğrudan etiket: model eğrisinin sağ ucunda.
        ax.annotate(f"{alt / 1000:g} km", (1000, model_nm(alt, 1000)), xytext=(8, 0),
                    textcoords="offset points", va="center", color=INK, fontsize=10)

    legend = [
        plt.Line2D([], [], color=INK_MUTED, linewidth=2, label="model"),
        plt.Line2D([], [], color=INK_MUTED, linewidth=1.5, linestyle=(0, (5, 4)), label=f"×{SAFETY} güvenli"),
        plt.Line2D([], [], color=INK_MUTED, marker="o", linestyle="", markersize=7, label="ölçülen"),
        plt.Line2D([], [], color=INK_MUTED, marker="o", linestyle="", markersize=7, markerfacecolor=SURFACE,
                   markeredgewidth=1.8, label=f"ölçülen, uyuma dahil değil (<{FIT_MIN_KT} kt)"),
    ]
    ax.legend(handles=legend, frameon=False, labelcolor=INK_MUTED, loc="upper right", fontsize=9)
    ax.set_xlim(400, 1060)
    ax.set_ylim(0, 30)
    fig.tight_layout()
    fig.savefig(HERE / "r_cold_model.png", dpi=150)
    plt.close(fig)


def chart_parity(rows):
    """Ölçülen – model: köşegen üstü = model eksik tahmin (tehlikeli taraf)."""
    fitted = [r for r in rows if r["v_escape_avg_kt"] >= FIT_MIN_KT]
    fig, ax = plt.subplots(figsize=(6.2, 5.8), facecolor=SURFACE)
    style(ax, "Ölçülen ve model karşılaştırması", "Model R_cold (NM)", "Ölçülen R_cold (NM)")

    limit = 24
    ax.plot([0, limit], [0, limit], color=INK_MUTED, linewidth=1.5, label="ölçülen = model")
    ax.plot([0, limit], [0, SAFETY * limit], color=INK_MUTED, linewidth=1.5, linestyle=(0, (5, 4)),
            label=f"güvenli formül ({SAFETY} × model)")
    for alt, color in ALT_COLORS.items():
        points = [r for r in fitted if r["alt_m"] == alt]
        ax.scatter([model_nm(alt, r["v_escape_avg_kt"]) for r in points], [r["r_cold_nm"] for r in points],
                   s=42, color=color, edgecolors=SURFACE, linewidths=1.5, zorder=3, label=f"{alt / 1000:g} km")
    ax.text(0.98, 0.03, "Bütün noktalar kesikli çizginin altında:\nformül × 1.15 ölçülenden hep büyük",
            transform=ax.transAxes, ha="right", va="bottom", color=INK_MUTED, fontsize=9)
    ax.legend(frameon=False, labelcolor=INK_MUTED, loc="upper left", fontsize=9)
    ax.set_xlim(0, limit)
    ax.set_ylim(0, limit)
    ax.set_aspect("equal")
    fig.tight_layout()
    fig.savefig(HERE / "r_cold_karsilastirma.png", dpi=150)
    plt.close(fig)


if __name__ == "__main__":
    data = load()
    chart_model(data)
    chart_parity(data)
    print("Yazıldı:", HERE / "r_cold_model.png", HERE / "r_cold_karsilastirma.png")
