"""Uçuş kaydındaki sütunları zamana göre çizer.

Workspace içinden:
  python src/sim/kayit_grafik.py recordings/dor_test_15
      Kaynakları ve sütunlarını listeler.
  python src/sim/kayit_grafik.py recordings/dor_test_15 KZL--blue:tas_kt Hostile--red:tas_kt
      Seçilen sütunları tek grafikte çizer.
  python src/sim/kayit_grafik.py recordings/dor_test_15 calc/blue:range_3d_nm calc/blue:mar_nm calc/blue:dor_nm --ayri
      --ayri: her seri ayrı alt grafikte, zaman ekseni ortak.
  ... --kaydet grafik.png
      Pencere açmak yerine dosyaya yazar.

Seri biçimi KAYNAK:SÜTUN
  KAYNAK: CSV adındaki parantez içi (KZL--blue, Hostile--red--0) ya da calc/<id> (calculations.csv'nin o uçak satırları).

Türetilmiş sütunlar, uçak ve füze dosyalarında:
  tas_ivme_kt_s:      kt/s, TAS'ın zamana göre türevi; + hızlanıyor
  tas_sarsinti_kt_s2: kt/s², ivmenin zamana göre türevi (jerk)
  yol_nm:             NM, kaydın ilk noktasından beri gidilen yol; füzede atıştan beri
  yol_2d_nm:          NM, aynı yolun yatay kısmı (irtifa değişimi sayılmaz)

Füze serilerinde (KZL--blue--0 gibi) iki an işaretlenir:
  pitbull: füze-hedef mesafesinin ilk kez PITBULL_NM'nin altına indiği an
  vurdu:   events.jsonl'deki missile_hit anı; isabet yoksa hedefe en yakın geçtiği an ve mesafesi
"""
import argparse
import json
import re
import sys
from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np
import pandas as pd
import pymap3d
from matplotlib.ticker import FuncFormatter

CALC_FILE = "calculations.csv"
EVENTS_FILE = "events.jsonl"
PITBULL_NM = 10.0   # src/otopilot/model/missile_profiles.hpp, AIM120C pitbull_nm
M_PER_NM = 1852.0
MISSILE_SOURCE = re.compile(r"^(.+)--(\d+)$")   # KZL--blue--0 → atan KZL--blue, slot 0


def sources(folder):
    """Kaynak adı → DataFrame. Boş CSV'ler (atılmamış füzeler) alınmaz."""
    found = {}
    for path in sorted(folder.glob("*.csv")):
        if path.name == CALC_FILE:
            calc = pd.read_csv(path)
            for aircraft_id, rows in calc.groupby("id"):
                found[f"calc/{aircraft_id}"] = rows
            continue
        data = pd.read_csv(path)
        if data.empty:
            continue
        name = path.name[path.name.find("(") + 1:path.name.find(")")] if "(" in path.name else path.stem
        if "tas_kt" in data.columns and len(data) > 1:
            # Türetilmiş: TAS'ın zamana göre değişimi; + hızlanıyor
            data["tas_ivme_kt_s"] = np.gradient(data["tas_kt"].to_numpy(), data["time_s"].to_numpy())
            # Türetilmiş: ivmenin zamana göre değişimi (sarsıntı, jerk)
            data["tas_sarsinti_kt_s2"] = np.gradient(data["tas_ivme_kt_s"].to_numpy(), data["time_s"].to_numpy())
        if {"lat_deg", "lon_deg", "alt_ft"} <= set(data.columns):
            # Türetilmiş: kaydın ilk noktasından beri gidilen yol (ardışık noktalar arası 3B uzaklıkların toplamı)
            steps_nm = np.linalg.norm(np.diff(ecef_m(data), axis=0), axis=1) / M_PER_NM
            data["yol_nm"] = np.concatenate([[0.0], np.cumsum(steps_nm)])
            # Türetilmiş: aynı yolun yatay kısmı; irtifa değişimi sayılmaz (noktalar aynı irtifaya indirilir)
            flat = ecef_m(data.assign(alt_ft=0.0))
            steps_2d_nm = np.linalg.norm(np.diff(flat, axis=0), axis=1) / M_PER_NM
            data["yol_2d_nm"] = np.concatenate([[0.0], np.cumsum(steps_2d_nm)])
        found[name] = data
    return found


def mmss(seconds, _position=None):
    return f"{int(seconds // 60)}:{int(seconds % 60):02d}"


def series(found, spec):
    source, _, column = spec.partition(":")
    if source not in found:
        sys.exit(f"Kaynak yok: {source}. Var olanlar: {', '.join(found)}")
    data = found[source]
    if column not in data.columns:
        sys.exit(f"{source} içinde sütun yok: {column}")
    values = pd.to_numeric(data[column], errors="coerce")
    if values.isna().all():
        sys.exit(f"{spec} sayı değil, çizilemez.")
    return data["time_s"], values


def ecef_m(data):
    x, y, z = pymap3d.geodetic2ecef(data["lat_deg"].to_numpy(), data["lon_deg"].to_numpy(), data["alt_ft"].to_numpy() * 0.3048)
    return np.column_stack([x, y, z])


def missile_events(folder, found, source):
    """Füze kaynağı için [(zaman, etiket)]; füze değilse boş."""
    match = MISSILE_SOURCE.match(source)
    if not match or match.group(1) not in found:
        return []
    shooter, slot = match.group(1), match.group(2)
    side = shooter.split("--")[-1]   # blue / red
    targets = [name for name in found if not name.startswith("calc/") and not MISSILE_SOURCE.match(name) and name != shooter]
    if len(targets) != 1:
        return []   # 1v1 dışında hedef belirsiz

    missile, target = found[source], found[targets[0]]
    t = missile["time_s"].to_numpy()
    target_pos = ecef_m(target)
    target_at_t = np.column_stack([np.interp(t, target["time_s"], target_pos[:, i]) for i in range(3)])
    range_nm = np.linalg.norm(ecef_m(missile) - target_at_t, axis=1) / M_PER_NM

    marks = []
    below = np.nonzero(range_nm < PITBULL_NM)[0]
    if below.size:
        marks.append((t[below[0]], "pitbull"))

    hit_time = None
    events_path = folder / EVENTS_FILE
    if events_path.exists():
        for line in events_path.read_text(encoding="utf-8").splitlines():
            event = json.loads(line)
            if event.get("type") == "missile_hit" and event.get("agent") == side and event.get("ammo") == slot:
                hit_time = event["sim_time_s"]
    closest = int(np.argmin(range_nm))
    if hit_time is not None:
        marks.append((hit_time, f"vurdu ({range_nm[closest]:.2f} NM)"))
    else:
        marks.append((t[closest], f"en yakın {range_nm[closest]:.2f} NM"))
    return marks


def main():
    parser = argparse.ArgumentParser(description="Uçuş kaydındaki sütunları zamana göre çizer.")
    parser.add_argument("kayit", type=Path, help="kayıt klasörü, ör. recordings/dor_test_15")
    parser.add_argument("seriler", nargs="*", help="KAYNAK:SÜTUN; boşsa kaynaklar ve sütunlar listelenir")
    parser.add_argument("--ayri", action="store_true", help="her seri ayrı alt grafikte")
    parser.add_argument("--kaydet", type=Path, help="pencere yerine bu dosyaya yaz (.png)")
    args = parser.parse_args()

    found = sources(args.kayit)
    if not args.seriler:
        for name, data in found.items():
            print(f"{name}:\n  {', '.join(c for c in data.columns if c != 'time_s')}")
        return

    count = len(args.seriler) if args.ayri else 1
    fig, axes = plt.subplots(count, 1, sharex=True, squeeze=False, figsize=(11, 2.6 * count + 1.5))
    for i, spec in enumerate(args.seriler):
        ax = axes[i if args.ayri else 0][0]
        time_s, values = series(found, spec)
        line, = ax.plot(time_s, values, label=spec, linewidth=1.2)
        for event_time, text in missile_events(args.kayit, found, spec.partition(":")[0]):
            value = np.interp(event_time, time_s, values)
            ax.axvline(event_time, color=line.get_color(), linestyle=":", linewidth=0.8)
            ax.plot(event_time, value, "o", color=line.get_color())
            ax.annotate(f"{text} {mmss(event_time)}", (event_time, value), textcoords="offset points",
                        xytext=(4, 6), fontsize=8, color=line.get_color())
        ax.grid(True, alpha=0.3)
        ax.legend(loc="best", fontsize=8)
    axes[-1][0].xaxis.set_major_formatter(FuncFormatter(mmss))
    axes[-1][0].set_xlabel("zaman (dk:sn)")
    fig.suptitle(args.kayit.name)
    fig.tight_layout()

    if args.kaydet:
        fig.savefig(args.kaydet, dpi=120)
        print(f"Yazıldı: {args.kaydet}")
    else:
        plt.show()


if __name__ == "__main__":
    main()
