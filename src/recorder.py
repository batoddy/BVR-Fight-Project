"""Ham JSBSim ve BVRGym verilerini hesaplama yapmadan CSV'ye kaydet."""
import csv
import re
from datetime import datetime
from pathlib import Path


# Yalnızca karar, uçuş sınırları ve temel uçuş teşhisi için gereken ham alanlar.
# Alan modelde yoksa eklenmez; sıfır değer uydurulmaz. Birim dönüşümü yapılmaz.
PHYSICS_FIELDS = (
    "simulation/sim-time-sec",
    "position/lat-geod-deg", "position/lat-gc-deg", "position/long-gc-deg",
    "position/h-sl-meters", "position/h-agl-ft", "position/terrain-elevation-asl-ft",
    "attitude/phi-deg", "attitude/theta-deg", "attitude/psi-deg",
    "velocities/v-north-fps", "velocities/v-east-fps", "velocities/v-down-fps",
    "velocities/u-fps", "velocities/v-fps", "velocities/w-fps",
    "velocities/p-rad_sec", "velocities/q-rad_sec", "velocities/r-rad_sec",
    "velocities/vt-fps", "velocities/vc-kts", "velocities/mach", "velocities/vg-fps",
    "aero/alpha-deg", "aero/beta-deg",
    "accelerations/n-pilot-x-norm", "accelerations/n-pilot-y-norm",
    "accelerations/n-pilot-z-norm", "accelerations/udot-ft_sec2",
    "accelerations/vdot-ft_sec2", "accelerations/wdot-ft_sec2",
    "accelerations/pdot-rad_sec2", "accelerations/qdot-rad_sec2",
    "accelerations/rdot-rad_sec2", "forces/load-factor",
    "forces/fbx-total-lbs", "forces/fby-total-lbs", "forces/fbz-total-lbs",
    "moments/l-total-lbsft", "moments/m-total-lbsft", "moments/n-total-lbsft",
    "atmosphere/T-R", "atmosphere/rho-slugs_ft3", "atmosphere/P-psf",
    "atmosphere/a-fps", "atmosphere/total-wind-north-fps",
    "atmosphere/total-wind-east-fps", "atmosphere/total-wind-down-fps",
    "inertia/mass-slugs", "inertia/weight-lbs",
    "propulsion/total-fuel-lbs", "propulsion/engine/thrust-lbs",
    "propulsion/engine/fuel-flow-rate-pps", "propulsion/engine/set-running",
    "fcs/throttle-cmd-norm", "fcs/aileron-cmd-norm", "fcs/elevator-cmd-norm",
    "fcs/rudder-cmd-norm", "fcs/left-aileron-pos-deg", "fcs/right-aileron-pos-deg",
    "fcs/elevator-pos-deg", "fcs/rudder-pos-deg", "fcs/flap-pos-deg",
    "fcs/speedbrake-cmd-norm", "fcs/speedbrake-pos-deg",
    "gear/wow", "gear/gear-pos-norm",
)


class CSVRecorder:
    def __init__(self, directory):
        self.directory = Path(directory) / datetime.now().strftime("%Y%m%d_%H%M%S_%f")
        self.directory.mkdir(parents=True)
        self.files = {}
        self.writers = {}
        self.properties = {}
        self.started_missiles = set()
        self.finished_missiles = set()
        self.manifest = self.directory / "KAYIT_ICERIGI.txt"
        self.manifest.write_text(
            "HAM SIMULASYON KAYDI\n"
            "Birim dönüşümü veya türetilmiş hesaplama yapılmaz.\n"
            "PHYSICS_FIELDS listesindeki mevcut R/RW propertyler kaydedilir; W alanlar okunmaz.\n"
            "Her record() çağrısı bir snapshot; bütün fizik alt adımları değildir.\n"
            "Füzenin simulation/sim-time-sec alanı kendi saatidir; launch/reset ile sıfırlanabilir.\n"
            "Füze CSV'leri başlıklarla başlangıçta oluşturulur; veri satırları atıştan sonra başlar.\n"
            "Kök klasördeki CSV'ler Tacview viewer uyumludur; BVRGym durum CSV'leri state/ altındadır.\n"
            "Tacview Time doğrudan taşıyıcının simülasyon saatidir; füzenin ham saati ayrıca korunur.\n"
            "Python None = boş CSV hücresi; bool = True/False.\n",
            encoding="utf-8",
        )

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_value, traceback):
        self.close()

    def close(self):
        for file in self.files.values():
            file.close()

    def _write(self, filename, values, source, subdirectory="", header_only=False):
        file_key = f"{subdirectory}/{filename}" if subdirectory else filename
        if file_key not in self.files:
            safe_name = re.sub(r'[<>:"/\\|?*\x00-\x1f]', '_', filename)
            folder = self.directory / subdirectory
            folder.mkdir(exist_ok=True)
            file = (folder / safe_name).open("x", newline="", encoding="utf-8")
            self.files[file_key] = file
            self.writers[file_key] = csv.DictWriter(file, fieldnames=list(values))
            self.writers[file_key].writeheader()
            with self.manifest.open("a", encoding="utf-8") as index:
                relative_path = str(Path(subdirectory) / safe_name)
                index.write(f"\n{'=' * 72}\nDOSYA: {relative_path}\nKAYNAK: {source}\n")
                index.write("SUTUNLAR:\n" + "\n".join(values) + "\n")
        if not header_only:
            self.writers[file_key].writerow(values)
        self.files[file_key].flush()

    def _physics(self, prefix, model, filename, time_s, header_only=False):
        if prefix not in self.properties:
            readable = set()
            for entry in model.fdm.get_property_catalog():
                name, permission = entry.rsplit(" (", 1)
                if "R" in permission:
                    readable.add(name)
            self.properties[prefix] = [name for name in PHYSICS_FIELDS if name in readable]
        raw = {
            name: model.fdm[name] for name in self.properties[prefix]
        }
        self._write(filename, {
            "Time": time_s,
            "Longitude": raw["position/long-gc-deg"],
            "Latitude": raw["position/lat-geod-deg"],
            "Altitude": raw["position/h-sl-meters"],
            "Roll (deg)": raw["attitude/phi-deg"],
            "Pitch (deg)": raw["attitude/theta-deg"],
            "Yaw (deg)": raw["attitude/psi-deg"],
            "Mach": raw["velocities/mach"],
            **raw,
        }, "Tacview sütun adları + seçilmiş ham JSBSim propertyleri; dönüşüm yok",
            header_only=header_only)

    def record(self, agents):
        for key, agent in agents.items():
            prefix = f"{key}__aircraft"
            name, team = agent.conf.agent_name, agent.conf.team
            time_s = agent.simObj.fdm["simulation/sim-time-sec"]
            self._physics(prefix, agent.simObj, f"F-16 ({name}--{key}) [{team}].csv", time_s)
            self._write(prefix + "__state.csv", {
                "simulation/sim-time-sec": agent.simObj.fdm["simulation/sim-time-sec"],
                "conf.agent_name": agent.conf.agent_name,
                "conf.team": agent.conf.team,
                "conf.aircraft_name": agent.conf.aircraft_name,
                "healthPoints": agent.healthPoints,
                "target.conf.agent_name": None if agent.target is None else agent.target.conf.agent_name,
            }, "BVRGym ajan alanları; zaman doğrudan JSBSim'den", subdirectory="state")
            for ammo_id, missile in agent.ammo.items():
                prefix = f"{key}__missile__{ammo_id}"
                if prefix in self.finished_missiles:
                    continue
                header_only = (prefix not in self.started_missiles
                               and not (missile.active or missile.target_hit or missile.target_lost))
                if header_only and prefix in self.properties:
                    continue  # Atılmamış füzenin dosyası zaten başlıklarla hazır.
                if not header_only:
                    self.started_missiles.add(prefix)
                self._physics(prefix, missile,
                              f"AIM-120 AMRAAM ({name}--{key}--{ammo_id}) [{team}].csv",
                              time_s, header_only=header_only)
                self._write(prefix + "__state.csv", {
                    "simulation/sim-time-sec": missile.fdm["simulation/sim-time-sec"],
                    "carrier.simulation/sim-time-sec": agent.simObj.fdm["simulation/sim-time-sec"],
                    "active": missile.active,
                    "alive": missile.alive,
                    "target_hit": missile.target_hit,
                    "target_lost": missile.target_lost,
                    "lost_count": missile.lost_count,
                    "target_norm": missile.target_norm,
                    "target_norm_old": missile.target_norm_old,
                    "target.conf.agent_name": None if missile.target is None else missile.target.conf.agent_name,
                }, "BVRGym füze alanları; zaman füzenin kendi JSBSim saatinden",
                    subdirectory="state", header_only=header_only)
                if header_only:
                    continue
                if not missile.active or not missile.alive or missile.target_hit or missile.target_lost:
                    self.finished_missiles.add(prefix)


# ============================================================================
# UÇAK FİZİĞİ → F-16 (<isim>--<ajan>) [<takım>].csv
# ============================================================================
# PHYSICS_FIELDS içindeki mevcut R/RW alanları, orijinal ad/değerleriyle.
# Konum, yönelim, hız, açısal hız, ivme/yük, toplam kuvvet/moment, alpha/beta,
# atmosfer/rüzgâr, kütle, yakıt/itki ve temel kumanda verileri tutulur.
# Yer teması ayrıntıları, çözücü/başlangıç ayarları, geometri/yerleşimler,
# aerodinamik katsayılar ve motor iç durumları kaydedilmez. Birimler değişmez:
# ör. velocities/vt-fps ft/s; propulsion/total-fuel-lbs lb. Getter/dönüşüm yok.
# Time, Longitude, Latitude, Altitude, Roll/Pitch/Yaw (deg), Mach sütunları
# aynı ham değerlerin arayüze uygun başlıklarıdır; hesaplama veya dönüşüm yoktur.
# TAS sütunu yazılmaz: m/s dönüşümü yerine ham velocities/vt-fps korunur.
#
# ============================================================================
# AJAN DURUMU → state/<ajan>__aircraft__state.csv
# ============================================================================
# conf.agent_name, conf.team, conf.aircraft_name, healthPoints,
# target.conf.agent_name ve simulation/sim-time-sec doğrudan okunur.
# Mühimmat sayımı, mesafe veya göreli açı hesabı yapılmaz.
#
# ============================================================================
# FÜZE FİZİĞİ → AIM-120 AMRAAM (<isim>--<ajan>--<anahtar>) [<takım>].csv
# ============================================================================
# PHYSICS_FIELDS içindeki mevcut ham alanlar, atıştan itibaren kaydedilir.
# Dosyalar başlangıçta başlıklarla oluşturulur; atılmadan veri satırı yazılmaz.
# Bitiş durumu bir kez yazılır; ardından bu füzenin kaydı durur.
# Tacview Time taşıyıcının saatidir; ham simulation/sim-time-sec füzenin saatidir.
#
# ============================================================================
# FÜZE DURUMU → state/<ajan>__missile__<mühimmat_anahtarı>__state.csv
# ============================================================================
# active, alive, target_hit, target_lost, lost_count, target_norm, target_norm_old,
# target.conf.agent_name ve simulation/sim-time-sec doğrudan okunur.
# carrier.simulation/sim-time-sec taşıyıcı uçağın doğrudan simülasyon saatidir;
# arayüz bu alanı kullanarak füze uçuşunu ortak zaman çizelgesine yerleştirebilir.
# target_norm BVRGym'in hesapladığı alandır; recorder yalnızca mevcut değeri yazar.
# is_active(), is_ready_to_launch() ve hesaplama/dönüşüm yapan getterlar kullanılmaz.
#
# ============================================================================
# DOSYA / ALAN İNDEKSİ → KAYIT_ICERIGI.txt
# ============================================================================
# Gerçek dosya isimleri, veri kaynakları ve tam sütun listeleri her kayıtta yazılır.
# CSV formatını bozmamak için bannerlar veri satırlarının içine eklenmez.
