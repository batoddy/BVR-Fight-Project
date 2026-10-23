"""Build an offline HTML guide from the maintained Markdown/CSV documents."""
from __future__ import annotations

import csv
from html import escape
import json
import os
from pathlib import Path
import re
from urllib.parse import unquote, urlsplit

import markdown
from markdown.extensions.toc import slugify_unicode
import otopilot_uml

ROOT = Path(__file__).resolve().parents[1]
DESTINATION = ROOT / "docs/PROJE_REHBERI.html"
DOCUMENTS = [
    ("proje", "Proje özeti", "README.md", "Başlangıç"),
    ("sistem", "Sistem mimarisi", "docs/mimari/BVR_SISTEM_MIMARISI.md", "Mimari"),
    ("cpp", "C++ sınıfları ve kararlar", "docs/mimari/OTOPILOT_MIMARISI.md", "Mimari"),
    ("python", "Python simülasyonu", "src/sim/README.md", "Mimari"),
    ("calistirma", "Kurulum ve çalıştırma", "docs/CALISTIRMA_KILAVUZU.md", "Uygulama"),
    ("kaynak", "C++ dosya haritası", "src/otopilot/README.md", "Uygulama"),
    ("gelistirme", "Otopilot geliştirme", "src/otopilot/KULLANIM.md", "Uygulama"),
    ("veri", "Veri sözleşmesi", "docs/veri_info/SIMULASYON_VERI_REFERANSI.md", "Veri"),
    ("tcp", "TCP mesaj örnekleri", "docs/mimari/mesaj_sablonlari/tcp/README.md", "Veri"),
    ("arayuz", "Görüntüleyici kullanımı", "BVRGym-Goruntuleyici/README.md", "Görüntüleyici"),
    ("arayuz-veri", "Görüntüleyici veri akışı", "BVRGym-Goruntuleyici/VERI_AKISI.md", "Görüntüleyici"),
    ("dogrulama", "Doğrulama ve testler", "BVRGym-Goruntuleyici/Dogrulama.md", "Görüntüleyici"),
]


class Diagram:
    """Small, deterministic SVG renderer; no network or browser-side renderer."""

    def __init__(self, uid, width, height, title):
        self.uid, self.width, self.height = uid, width, height
        self.title = title
        self.edges, self.nodes = [], []

    def text(self, x, y, content, css="edge-label", anchor="middle"):
        return (f'<text x="{x}" y="{y}" class="{css}" text-anchor="{anchor}">'
                f'{escape(content)}</text>')

    def node(self, x, y, width, title, rows, tone="blue", kind="class",
             link="#cpp", height=None):
        height = height or (64 + len(rows) * 23)
        body = [
            f'<a href="{escape(link)}" class="diagram-node">',
            f'<g class="node {tone}">',
            f'<rect x="{x}" y="{y}" width="{width}" height="{height}" rx="9"/>',
            f'<path class="node-rule" d="M {x} {y+54} H {x+width}"/>',
            self.text(x+16, y+20, "«" + kind + "»", "node-kind", "start"),
            self.text(x+16, y+42, title, "node-title", "start"),
        ]
        for index, row in enumerate(rows):
            body.append(self.text(x+16, y+79+23*index, row, "node-row", "start"))
        body.append("</g></a>")
        self.nodes.append("".join(body))
        return height

    def edge(self, points, label="", label_at=None, kind="use", multiplicity=""):
        coords = " ".join(f"{x},{y}" for x, y in points)
        start = f' marker-start="url(#{self.uid}-diamond)"' if kind == "owns" else ""
        end = "" if kind == "owns" else (
            f' marker-end="url(#{self.uid}-triangle)"' if kind == "inherit"
            else f' marker-end="url(#{self.uid}-arrow)"')
        self.edges.append(f'<polyline class="edge {kind}" points="{coords}"{start}{end}/>')
        if label:
            x, y = label_at or points[len(points)//2]
            self.edges.append(self.text(x, y, label))
        if multiplicity:
            x, y = points[-1]
            self.edges.append(self.text(x+15, y-10, multiplicity, "multiplicity", "start"))

    def note(self, x, y, lines):
        for index, line in enumerate(lines):
            self.nodes.append(self.text(x, y+21*index, line, "diagram-note", "start"))

    def render(self):
        return (
            f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {self.width} {self.height}" '
            f'role="img" aria-labelledby="{self.uid}-title" data-width="{self.width}">'
            f'<title id="{self.uid}-title">{escape(self.title)}</title>'
            '<defs>'
            f'<marker id="{self.uid}-arrow" viewBox="0 0 12 12" refX="11" refY="6" '
            'markerWidth="11" markerHeight="11" markerUnits="userSpaceOnUse" orient="auto">'
            '<path d="M1 1 L11 6 L1 11" fill="none" stroke="#63738a" stroke-width="1.8"/></marker>'
            f'<marker id="{self.uid}-diamond" viewBox="0 0 15 10" refX="0" refY="5" '
            'markerWidth="15" markerHeight="10" markerUnits="userSpaceOnUse" orient="auto">'
            '<path d="M0 5 L7 0 L14 5 L7 10 Z" fill="#243750"/></marker>'
            f'<marker id="{self.uid}-triangle" viewBox="0 0 15 14" refX="14" refY="7" '
            'markerWidth="15" markerHeight="14" markerUnits="userSpaceOnUse" orient="auto">'
            '<path d="M1 1 L14 7 L1 13 Z" fill="white" stroke="#63738a" stroke-width="1.5"/></marker>'
            '</defs>'
            + "".join(self.edges) + "".join(self.nodes) + "</svg>"
        )


def diagram(kind, uid):
    if kind == "system":
        d = Diagram(uid, 1120, 830, "Sistem bileşenleri ve veri yolları")
        d.node(55, 20, 330, "scenario.yaml", ["Süre · başlangıç · bağlantı"], "slate", "dosya", "#calistirma", 100)
        d.node(55, 180, 330, "Python simülasyonu", ["sim_main.py → Simulation", "Ölçüm okur · komutu uygular", "Fizik nesnelerinin sahibidir"], "green", "process", "#python", 150)
        d.node(725, 180, 340, "C++ / Sim", ["World.update()", "Fighter ve hostile otopilotları", "Uçuş hedefi · atış · hesap"], "blue", "process", "#cpp", 150)
        d.node(55, 410, 330, "BVRGym / JSBSim", ["Uçak ve füze fiziği", "Yön/irtifa → kontrol yüzeyleri"], "green", "altyapı", "#python", 120)
        d.node(725, 410, 340, "Recorder", ["Ham ölçüm + olay", "C++ hesabını aynı zamanla yazar"], "amber", "recording", "#veri", 120)
        d.node(725, 610, 340, "Oturum klasörü", ["CSV · calculations.csv", "events.jsonl · manifest.json"], "amber", "files", "#veri", 120)
        d.node(55, 610, 330, "Görüntüleyici", ["RecordingStore → HTTP", "Tarayıcı: 2D / 3D + analiz"], "violet", "process", "#arayuz-veri", 120)
        d.edge([(220, 120), (220, 180)], "kurulum", (265, 156))
        d.edge([(385, 225), (725, 225)], "state: ölçüm + önceki komut", (555, 211))
        d.edge([(725, 294), (385, 294)], "command: iki uçak + atış + hesap", (555, 280))
        d.edge([(220, 330), (220, 410)], "oku / ilerlet", (285, 378))
        d.edge([(385, 330), (535, 365), (535, 470), (725, 470)], "snapshot + hesap", (610, 452))
        d.edge([(895, 530), (895, 610)], "dosyaya yaz", (960, 574))
        d.edge([(725, 671), (385, 671)], "kayıt dosyalarını oku", (555, 655))
        d.note(55, 785, ["TCP yalnız Python ↔ C++ arasında. Görüntüleyici kayıt üzerinden çalışır; uçuş komutu yolu yoktur."])
    elif kind == "classes":
        d = Diagram(uid, 1130, 920, "Sim, World ve iki otopilotun UML sahiplik ilişkileri")
        d.node(30, 25, 300, "main()", ["Connection ve Sim oluşturur", "receive → step → send"], "slate", "giriş", "#kaynak", 125)
        d.node(455, 25, 320, "Sim", ["- world_: World", "- otopilot_fighter_", "- otopilot_hostile_", "+ step(state): Message"], "blue", "class", "#cpp", 175)
        d.node(30, 220, 300, "Connection", ["+ receive(state)", "+ send(command)"], "slate", "class", "#cpp", 120)
        d.node(30, 395, 300, "OtopilotFighter", ["- state_: int", "- command_: FlightCommand", "- maneuvers_: Maneuvers", "+ step(own, world, current)"], "blue", "class", "#cpp", 180)
        d.node(420, 395, 340, "World", ["+ fighter: Aircraft", "+ hostile: Aircraft", "+ sim_time_s", "+ update()"], "green", "struct", "#cpp", 180)
        d.node(835, 395, 280, "OtopilotHostile", ["- state_: int", "- command_: FlightCommand", "- maneuvers_: Maneuvers", "+ step(own, world, current)"], "blue", "class", "#cpp", 180)
        d.node(40, 735, 280, "Maneuvers", ["Fighter'a ait ayrı nesneler"], "blue", "instance", "#gelistirme", 105)
        d.node(420, 735, 340, "Aircraft × 2", ["fighter / hostile", "Ölçüm · profil · füze · hesap"], "green", "instances", "#cpp", 125)
        d.node(835, 735, 280, "Maneuvers", ["Hostile'e ait ayrı nesneler"], "blue", "instance", "#gelistirme", 105)
        d.edge([(330, 90), (455, 90)], "step(state)", (390, 76))
        d.edge([(180, 150), (180, 220)], "al / gönder", (245, 188))
        d.edge([(615, 200), (615, 395)], "world_", (658, 301), "owns")
        d.edge([(455, 160), (375, 160), (375, 360), (180, 360), (180, 395)],
               "otopilot_fighter_", (250, 348), "owns")
        d.edge([(775, 160), (805, 160), (805, 360), (975, 360), (975, 395)],
               "otopilot_hostile_", (950, 348), "owns")
        d.edge([(330, 485), (420, 485)], "world", (375, 471), "read")
        d.edge([(835, 485), (760, 485)], "world", (798, 471), "read")
        d.edge([(180, 575), (180, 735)], "maneuvers_", (245, 663), "owns")
        d.edge([(590, 575), (590, 735)], "fighter / hostile", (668, 663), "owns", "2")
        d.edge([(975, 575), (975, 735)], "maneuvers_", (1040, 663), "owns")
        d.note(35, 894, ["◆ sahiplik / üye nesne     - - → salt okunur World erişimi     → çağrı / veri kullanımı"])
    elif kind == "aircraft":
        d = Diagram(uid, 1150, 830, "Aircraft veri modeli ve iç içe nesnelerin UML ilişkileri")
        d.node(390, 25, 370, "Aircraft", ["+ id / name / team / type", "+ profile · kinematics · enemy", "+ missiles: vector<Missile>", "+ fire_missile() / last_fired()", "+ get_out_distance()"], "green", "struct", "#cpp", 200)
        nodes = [
            (20, "Kinematics", ["lat / lon / altitude", "heading / pitch / roll", "TAS · Mach · NED hız"], "green"),
            (305, "AircraftProfile", ["loadout: MissileProfile[]", "radar / performans", "out_g / altitude factors"], "slate"),
            (590, "EnemyInfo", ["range / ATA / bearing", "closure / LOS", "WEZ / MAR / DOR"], "amber"),
            (875, "Missile", ["slot / profile / fired", "python_id / target_id", "state / outcome / kinematics"], "green"),
        ]
        for x, title, rows, tone in nodes:
            d.node(x, 375, 255, title, rows, tone, "struct", "#cpp", 155)
        d.node(305, 675, 255, "MissileProfile", ["type / range / speed", "pitbull_range_m"], "slate", "struct", "#cpp", 120)
        d.node(875, 675, 255, "MissileTargetInfo", ["range / closure / time-to-go", "pitbull"], "amber", "struct", "#cpp", 120)
        for index, center in enumerate((147, 432, 717, 1002)):
            start_x = 430 + index * 95
            d.edge([(start_x, 225), (start_x, 290+index*16), (center, 290+index*16), (center, 375)],
                   kind="owns", multiplicity="0..*" if index == 3 else "1")
        d.edge([(432, 530), (432, 675)], "loadout", (485, 610), "owns", "0..*")
        d.edge([(1002, 530), (1002, 675)], "target", (1045, 610), "owns", "1")
        d.edge([(875, 492), (850, 492), (850, 633), (560, 633), (560, 710)],
               "profile: MissileProfile", (705, 622), "owns")
        d.note(24, 590, ["Yeşil: ölçüm ve durum", "Gri: model parametreleri", "Sarı: hesaplanan bilgi"])
    elif kind == "sequence":
        d = Diagram(uid, 1120, 850, "Bir dış adımın UML sequence diyagramı")
        lanes = [(130, "Python"), (430, "C++ / Sim"), (730, "Recorder"), (1010, "BVRGym")]
        for x, title in lanes:
            d.node(x-95, 15, 190, title, [], "slate", "lifeline", "#sistem", 65)
            d.edges.append(f'<line class="lifeline" x1="{x}" y1="80" x2="{x}" y2="820"/>')
        events = [
            (130, 430, 140, "state(t): ölçüm ve mevcut hedefler", "use"),
            (430, 430, 205, "StateReader → World.update()", "use"),
            (430, 430, 270, "İki otopilot → yeni komutlar", "use"),
            (430, 130, 340, "command(t): uçuş + atış + hesap", "use"),
            (130, 730, 410, "Hesabı mevcut ölçümün zamanıyla yaz", "use"),
            (130, 1010, 480, "Varsa açık atış emirlerini uygula", "use"),
            (130, 1010, 550, "Uçuş komutlarını doğrula ve ilerlet", "use"),
            (1010, 130, 615, "Yeni fizik durumu; tick = t + 1", "read"),
            (130, 730, 685, "Yeni ölçümü ve uygulanan komutu kaydet", "use"),
            (130, 430, 790, "Süre dolduğunda: end(son_tick)", "use"),
        ]
        for start, finish, y, label, style in events:
            points = [(start, y), (finish, y)] if start != finish else [
                (start, y), (start+70, y), (start+70, y+25), (start, y+25)]
            d.edge(points, label, ((start+finish)/2 if start != finish else start+210, y-13), style)
        d.note(42, 748, ["Üstteki adımlar süre dolana kadar yinelenir. Komut beklenirken fizik ilerlemez."])
    elif kind == "missile":
        d = Diagram(uid, 1140, 540, "Füze yaşam döngüsü ve C++ eşleştirmesi")
        d.node(20, 140, 240, "Ready", ["Python state = ready", "C++ fired = false", "Kinematik henüz yok"], "slate", "state", "#cpp", 155)
        d.node(305, 140, 240, "Atış bekleniyor", ["C++ fired = true", "python_id henüz boş", "launch_order = true"], "blue", "logical state", "#cpp", 155)
        d.node(590, 140, 240, "InFlight", ["Python kimliği eşleşti", "Hedef ve kinematik var", "World hedef hesabı yapar"], "green", "state", "#cpp", 155)
        d.node(875, 140, 240, "Ended", ["Uçuş sona erdi", "outcome: hit / lost", "Son ölçüm korunur"], "amber", "state", "#cpp", 155)
        d.edge([(260, 216), (305, 216)], "fire()", (282, 202))
        d.edge([(545, 216), (590, 216)], "ölçüm", (567, 202))
        d.edge([(830, 216), (875, 216)], "bitiş", (852, 202))
        d.note(25, 65, ["Pending/atış bekleniyor ayrı bir Python enum'u değildir; C++ tarafındaki eşleştirme durumudur."])
        d.note(25, 368, ["1. C++ atış isteğini işaretler; Python hazır ilk fizik slotunu fırlatır.",
                         "2. StateReader, yeni Python kimliğini bekleyen en eski C++ atışıyla eşleştirir.",
                         "3. World; füze ve hedefin durumundan mesafe, kapanma, süre ve pitbull tahmini üretir.",
                         "4. Kayıt aralığının bitmesi tek başına isabet değildir; sonucu olay/durum bildirir."])
    elif kind == "maneuvers":
        d = Diagram(uid, 1140, 770, "Maneuver arayüzü, kalıtım ve otopilot başına sahiplik")
        d.node(385, 25, 370, "Maneuver", ["+ compute(own, current)", "→ FlightCommand (virtual)"], "blue", "abstract", "#gelistirme", 120)
        names = ["Hold", "In", "Out", "Crank", "Notch"]
        rows = [["Komutu koru"], ["Hedefe yönel"], ["Ters yön iste"],
                ["side / kp / offset", "Radar sınırında tut"], ["Dik yön iste"]]
        for index, name in enumerate(names):
            x = 20+225*index
            d.node(x, 340, 205, name, rows[index], "blue", "class", "#gelistirme", 125)
            d.edge([(x+102, 340), (x+102, 245), (570, 245), (570, 145)], kind="inherit")
        d.node(345, 625, 450, "Maneuvers", ["hold · in · out · notch", "crank_left · crank_right"], "green", "struct", "#gelistirme", 120)
        for index in range(5):
            x = 122+225*index
            d.edge([(410+80*index, 625), (410+80*index, 550), (x, 550), (x, 465)], kind="owns")
        d.note(30, 195, ["Boş üçgen: ortak arayüzü uygular."])
        d.note(30, 595, ["Her otopilotun ayrı Maneuvers nesnesi vardır. Crank için sol ve sağ ayrı örneklerdir."])
    elif kind == "viewer":
        d = Diagram(uid, 1130, 740, "Görüntüleyici modülleri ve kayıt akışı")
        for x, title, rows, tone, y, link in [
            (25, "Kayıt dizini", ["Nesne CSV'leri", "Hesap · olay · manifest"], "amber", 30, "#veri"),
            (420, "recordings.py", ["Oku / doğrula / eşleştir", "WGS84 → yerel koordinat"], "green", 30, "#arayuz-veri"),
            (815, "RecordingStore", ["Geçerli veri + oturum", "Dizin izleme ve katalog"], "green", 30, "#arayuz-veri"),
            (815, "server.py", ["/api/state: 2000 örnek", "Kayıt açma / silme API"], "violet", 305, "#arayuz-veri"),
            (420, "SampleBuffer", ["stream.js", "Tampon / interpolasyon"], "violet", 305, "#arayuz-veri"),
            (25, "app.js", ["Zaman / seçim / paneller", "Kayıtlı C++ analiz bakışı"], "violet", 305, "#arayuz"),
            (25, "scene.js / scene2d.js", ["3D ve 2D çizim", "Hedef / radar / WEZ"], "violet", 575, "#arayuz"),
            (420, "basemap.js", ["İsteğe bağlı OSM görselleri", "Aynı yerel referans"], "slate", 575, "#arayuz"),
        ]:
            d.node(x, y, 290, title, rows, tone, "module", link, 120)
        d.edge([(315, 90), (420, 90)], "oku", (366, 77))
        d.edge([(710, 90), (815, 90)], "yayınla", (762, 77))
        d.edge([(960, 150), (960, 305)], "paket", (1002, 234))
        d.edge([(815, 365), (710, 365)], "HTTP", (762, 352))
        d.edge([(420, 365), (315, 365)], "örnek", (367, 352))
        d.edge([(170, 425), (170, 575)], "çiz", (205, 510))
        d.edge([(420, 635), (315, 635)], "zemin", (367, 622))
        d.note(815, 575, ["Kayıt dosyaları okunur.", "Yalnız onaylı silme diski değiştirir.", "Simülasyona komut gönderilmez."])
    else:
        raise ValueError(kind)
    return d.render()


DIAGRAMS = [
    ("system", "Sistem", "Üç süreç, tek fizik sahibi",
     "Python fizik ve kaydı yönetir. C++ karar verir. Görüntüleyici yalnız kayıt yolundan beslenir."),
    ("classes", "C++ sahipliği", "Hangi nesne kime ait?",
     "Sim, World ve iki otopilotu üye olarak tutar. Kesikli world okları const World& erişimini gösterir."),
    ("aircraft", "Uçak verisi", "Ölçüm, profil ve hesap ayrı",
     "Aircraft içindeki değer nesneleri ve füze listesi. Aynı sınıf iki uçak için ayrı örneklenir."),
    ("sequence", "Adım sırası", "Bir tick içinde neler olur?",
     "Ölçüm → karar → hesap kaydı → atış → fizik → yeni ölçüm. Sıra, hesapların doğru konuma bağlanmasını sağlar."),
    ("missile", "Füze yaşamı", "Atış isteğinden fizik sonucuna",
     "C++ atış işareti, Python fizik durumu ve hedefe göre hesap aynı kavram değildir."),
    ("maneuvers", "Manevralar", "Ortak arayüz, ayrı örnekler",
     "Boş üçgen kalıtımı; dolu elmas üye sahipliğini gösterir. İki otopilot manevra hafızasını paylaşmaz."),
    ("viewer", "Görüntüleyici", "Dosyadan tarayıcı sahnesine",
     "Kayıtları okuma, tam zamanda eşleme, HTTP aktarımı, interpolasyon ve çizim birbirinden ayrıdır."),
]


def figure(kind, uid, caption=None):
    info = next(item for item in DIAGRAMS if item[0] == kind)
    return (
        '<figure class="diagram-figure">'
        '<div class="figure-tools"><span>UML / bağlantı diyagramı</span>'
        '<div><button type="button" data-diagram="out" aria-label="Diyagramı küçült">−</button>'
        '<button type="button" data-diagram="reset" aria-label="Diyagramı sığdır">Sığdır</button>'
        '<button type="button" data-diagram="in" aria-label="Diyagramı büyüt">+</button>'
        '<button type="button" data-diagram="expand">Genişlet ↗</button></div></div>'
        '<div class="diagram-scroll" tabindex="0" aria-label="Kaydırılabilir diyagram">'
        + diagram(kind, uid) + '</div><figcaption>' + escape(caption or info[3])
        + ' Kutulara tıklayarak ilgili açıklamaya geçebilirsin.</figcaption></figure>'
    )


class LinkRewriter:
    def __init__(self):
        self.targets = {(ROOT / filename).resolve(): key for key, _, filename, _ in DOCUMENTS}
        self.targets[(ROOT / "docs/README.md").resolve()] = "overview"
        self.targets[(ROOT / "docs/mimari/mesaj_sablonlari/README.md").resolve()] = "tcp"

    def rewrite(self, html, source, page_key):
        def replace(match):
            href = unquote(match.group(1))
            parts = urlsplit(href)
            if parts.scheme or href.startswith("//"):
                return match.group(0)
            if not parts.path and parts.fragment in {"overview", "alanlar", "otopilot-uml", "otopilot-members", *self.targets.values()}:
                return match.group(0)
            path = (source.parent / parts.path).resolve() if parts.path else source.resolve()
            if path in self.targets:
                target = self.targets[path]
                suffix = "-" + parts.fragment if parts.fragment else ""
                return 'href="#' + target + suffix + '"'
            relative = os.path.relpath(path, DESTINATION.parent).replace("\\", "/")
            if parts.fragment:
                relative += "#" + parts.fragment
            return 'href="' + escape(relative, quote=True) + '"'
        return re.sub(r'href="([^"]*)"', replace, html)


def render_document(key, title, filename, links):
    source = ROOT / filename
    text = source.read_text(encoding="utf-8")
    text = re.sub(r"^# .*\n", "", text, count=1)
    index = 0

    def mermaid(match):
        nonlocal index
        mapping = {
            "proje": ["system"], "sistem": ["system", "sequence"],
            "cpp": ["classes"], "arayuz-veri": ["viewer"],
        }
        choices = mapping.get(key, [])
        if index >= len(choices):
            raise ValueError(f"Unmapped Mermaid block: {filename} #{index}")
        output = figure(choices[index], f"{key}-embedded-{index}")
        index += 1
        return "\n\n" + output + "\n\n"

    fence = chr(96) * 3
    text = re.sub(fence + r"mermaid\s*\n(.*?)" + fence, mermaid, text, flags=re.S)
    converter = markdown.Markdown(
        extensions=["tables", "fenced_code", "toc", "sane_lists"],
        extension_configs={"toc": {
            "slugify": lambda value, sep: key + "-" + slugify_unicode(value, sep),
        }},
    )
    body = converter.convert(text)
    body = re.sub(r"<(/?)h([1-5])\b", lambda m: "<" + m[1] + "h" + str(int(m[2])+1), body)
    body = links.rewrite(body, source, key)
    body = re.sub(r"(<table>.*?</table>)", r'<div class="table-scroll">\1</div>', body, flags=re.S)
    relative = os.path.relpath(source, DESTINATION.parent).replace("\\", "/")
    source_line = (
        '<div class="chapter-source">Kaynak: <a href="' + escape(relative) + '">'
        + escape(filename) + '</a></div>'
    )
    return (f'<section class="chapter" id="{key}" data-title="{escape(title)}">'
            '<div class="chapter-heading"><span class="eyebrow">BVR / Teknik rehber</span>'
            f'<h2>{escape(title)}</h2>{source_line}</div><div class="prose">{body}</div></section>')


def data_section():
    path = ROOT / "docs/veri_info/BVRGYM_VERI_LISTESI.csv"
    with path.open(encoding="utf-8", newline="") as handle:
        rows = list(csv.DictReader(handle))
    labels = {"veri": "Alan", "grup": "Grup", "tip": "Tip", "birim": "Birim",
              "kaynak": "Kaynak", "okuma": "Okuyan / üreten", "guncellenme": "Güncellenme",
              "aktarim": "Aktarım", "aciklama": "Açıklama"}
    head = "".join("<th scope=\"col\">" + label + "</th>" for label in labels.values())
    body = []
    for row in rows:
        cells = "".join("<td>" + escape(row.get(key, "")) + "</td>" for key in labels)
        body.append("<tr>" + cells + "</tr>")
    return (
        '<section class="chapter" id="alanlar" data-title="Alan kataloğu">'
        '<div class="chapter-heading"><span class="eyebrow">Veri / Kaynak ilişkisi</span>'
        '<h2>Alan kataloğu</h2><p>Bir değerin nereden geldiğini, hangi birimde olduğunu ve hangi kanaldan taşındığını bul.</p></div>'
        '<label class="catalog-search">Alanlarda ara<input id="field-search" type="search" '
        'placeholder="Örneğin: ATA, TCP, hız, time_s…" aria-controls="field-table"></label>'
        f'<p id="field-count" class="muted" aria-live="polite">{len(rows)} alan satırı</p>'
        '<div class="table-scroll catalog"><table id="field-table"><thead><tr>' + head
        + "</tr></thead><tbody>" + "".join(body) + "</tbody></table></div>"
        '<p class="muted">Aynı alan farklı kanallarda farklı birimle bulunabilir. '
        'Tam property envanteri: <a href="veri_info/JSBSIM_F16_PROPERTY_KATALOGU.csv">F-16 CSV</a> · '
        '<a href="veri_info/JSBSIM_FUZE_PROPERTY_KATALOGU.csv">Füze CSV</a>.</p></section>'
    )


def message_examples():
    blocks = []
    for name in ("state", "command", "end"):
        source = ROOT / "docs/mimari/mesaj_sablonlari/tcp" / (name + ".json")
        data = json.loads(source.read_text(encoding="utf-8"))
        pretty = json.dumps(data, ensure_ascii=False, indent=2)
        blocks.append(
            '<details class="json-example"><summary><strong>' + name
            + '</strong><span>tick ' + str(data["tick"]) + ' · JSON örneği</span></summary>'
            '<pre><code class="language-json">' + escape(pretty) + "</code></pre></details>")
    return "".join(blocks)


CSS = r"""
:root{--ink:#182941;--muted:#617187;--line:#dce3eb;--paper:#fff;--bg:#f3f6fa;--blue:#235bc1;--nav:#132239;--green:#17765b;--amber:#9a6518;--violet:#7253a8}
*{box-sizing:border-box}html{scroll-behavior:smooth;scroll-padding-top:25px}body{margin:0;background:var(--bg);color:var(--ink);font:15px/1.75 "Segoe UI",Arial,sans-serif}a{color:var(--blue);text-underline-offset:3px}button,input{font:inherit}button{cursor:pointer}button:focus-visible,a:focus-visible,input:focus-visible,summary:focus-visible{outline:3px solid #5e9bef;outline-offset:3px}button{border:1px solid var(--line);border-radius:7px;background:#fff;color:var(--ink);padding:6px 12px}button:hover{background:#eef3fb}button[aria-pressed=true]{background:var(--ink);border-color:var(--ink);color:white}[hidden]{display:none!important}
.sidebar{position:fixed;inset:0 auto 0 0;width:275px;background:var(--nav);color:#e8edf5;display:flex;flex-direction:column;z-index:5}.brand{padding:29px 25px 22px;border-bottom:1px solid #ffffff15}.brand strong{display:block;letter-spacing:.12em;font-size:17px}.brand span{color:#9eafc6;font-size:12px}.nav-scroll{padding:16px 15px;overflow:auto;flex:1}.nav-group{margin:19px 10px 7px;font-size:10px;text-transform:uppercase;letter-spacing:.16em;color:#92a5bf}.nav-link{display:flex;align-items:center;gap:10px;padding:9px 11px;margin:3px 0;border-radius:7px;text-decoration:none;color:#c6d1e0;font-size:13px;line-height:1.45}.nav-link b{width:22px;color:#849bb9;font-size:10px;font-weight:500}.nav-link:hover{background:#ffffff0b;color:#fff}.nav-link.active{background:#284264;color:#fff}.nav-link.active b{color:#9dc5ff}.sidebar-foot{padding:16px 25px;border-top:1px solid #ffffff15;font-size:11px;color:#9eafc6}
.main{margin-left:275px;max-width:1630px;padding:30px 42px 70px}.topbar{display:flex;gap:15px;align-items:center;justify-content:space-between;margin-bottom:26px}.search{flex:1;max-width:580px;position:relative}.search label{display:block;color:var(--muted);font-size:11px;margin-bottom:4px}.search input,.catalog-search input{width:100%;border:1px solid var(--line);border-radius:8px;padding:10px 13px;background:white;color:var(--ink)}.topbar-actions{display:flex;gap:8px;align-items:center;flex-wrap:wrap}.topbar-actions button{font-size:12px}.search-status{color:var(--muted);font-size:12px;min-height:20px}.eyebrow{font-size:10px;font-weight:700;letter-spacing:.16em;text-transform:uppercase;color:var(--blue)}.hero{padding:30px 0 20px}.hero h1{font-size:clamp(30px,3.6vw,51px);line-height:1.14;letter-spacing:-.04em;margin:12px 0 20px;font-weight:650}.hero p{max-width:760px;color:var(--muted);font-size:17px;line-height:1.75}.chips{display:flex;gap:8px;flex-wrap:wrap}.chip{border:1px solid var(--line);border-radius:30px;background:#fff;padding:4px 10px;font-size:11px;color:var(--muted)}.facts{display:grid;grid-template-columns:repeat(3,minmax(0,1fr));gap:14px;margin:20px 0 34px}.fact{padding:18px 21px;border-radius:9px;background:white;border:1px solid var(--line);border-top:3px solid var(--blue)}.fact.green{border-top-color:var(--green)}.fact.violet{border-top-color:var(--violet)}.fact span{font-size:10px;letter-spacing:.1em;text-transform:uppercase;color:var(--muted)}.fact strong{display:block;font-size:17px;margin:4px 0 6px}.fact p{margin:0;color:var(--muted);font-size:12px;line-height:1.6}
.map-header{display:flex;align-items:flex-end;justify-content:space-between;gap:20px}.map-header h2{margin:0;font-size:25px;letter-spacing:-.025em}.map-header p{margin:4px 0 16px;font-size:13px;color:var(--muted)}.diagram-tabs{display:flex;gap:6px;flex-wrap:wrap;margin:10px 0 18px}.diagram-tabs button{font-size:12px;padding:7px 10px}.diagram-description h3{font-size:19px;margin:18px 0 3px}.diagram-description p{margin:0 0 15px;font-size:13px;color:var(--muted)}.diagram-figure{margin:15px 0 30px;border:1px solid var(--line);background:#fff;border-radius:10px;overflow:hidden}.figure-tools{display:flex;align-items:center;justify-content:space-between;gap:12px;padding:10px 16px;background:#f8fafc;border-bottom:1px solid var(--line)}.figure-tools>span{font-size:11px;letter-spacing:.04em;color:var(--muted)}.figure-tools button{font-size:11px;min-width:30px;padding:4px 8px}.figure-tools div{display:flex;gap:5px}.diagram-scroll{overflow:auto;padding:22px 12px;max-height:820px;background:#fff}.diagram-scroll svg{width:100%;height:auto;display:block;min-width:680px}.diagram-figure figcaption{border-top:1px solid var(--line);color:var(--muted);font-size:12px;padding:12px 16px;line-height:1.65}.edge{fill:none;stroke:#63738a;stroke-width:1.6;stroke-linejoin:round}.edge.read{stroke-dasharray:6 5}.edge-label{font:12px "Segoe UI",Arial,sans-serif;fill:#465973;paint-order:stroke;stroke:#fff;stroke-width:5px;stroke-linejoin:round}.multiplicity{font:11px "Segoe UI",Arial,sans-serif;fill:#465973;paint-order:stroke;stroke:#fff;stroke-width:4px}.lifeline{stroke:#b9c5d5;stroke-dasharray:5 6;stroke-width:1.3}.node rect{fill:#f5f8ff;stroke:#7199d5;stroke-width:1.4}.node-rule{stroke:#c6d7ef;stroke-width:1}.node.green rect{fill:#f0faf6;stroke:#68a994}.node.green .node-rule{stroke:#c3e1d6}.node.amber rect{fill:#fffbf1;stroke:#d5b16f}.node.amber .node-rule{stroke:#e9d8b7}.node.violet rect{fill:#f8f4ff;stroke:#a38ac8}.node.violet .node-rule{stroke:#dbcfeb}.node.slate rect{fill:#f5f7fa;stroke:#9badc2}.node.slate .node-rule{stroke:#d2dce7}.node-kind{font:10px "Segoe UI",Arial,sans-serif;fill:#65768c;letter-spacing:1px}.node-title{font:600 16px "Segoe UI",Arial,sans-serif;fill:#21334c}.node-row{font:12px Consolas,"Segoe UI",monospace;fill:#42536b}.diagram-note{font:12px "Segoe UI",Arial,sans-serif;fill:#617187}.diagram-node{text-decoration:none}.diagram-node:hover rect{stroke:#235bc1;stroke-width:2.4}
.reading-path{background:#eaf0f9;border:1px solid #d2dff1;border-radius:10px;padding:21px 24px;margin:25px 0}.reading-path h3{margin:0 0 7px;font-size:16px}.reading-path p{font-size:13px;margin:0;color:#526783}.reading-links{display:flex;gap:16px;flex-wrap:wrap;margin-top:13px;font-size:13px}.chapter{background:#fff;border:1px solid var(--line);border-radius:12px;padding:30px 36px;margin:24px 0 36px;scroll-margin-top:24px}.chapter-heading{border-bottom:1px solid var(--line);margin-bottom:26px;padding-bottom:22px}.chapter-heading h2{font-size:29px;line-height:1.3;letter-spacing:-.025em;margin:7px 0 12px}.chapter-heading p{font-size:14px;color:var(--muted);max-width:770px}.chapter-source{font-size:11px;color:var(--muted);overflow-wrap:anywhere}.chapter-source a{color:#657991}.prose{font-size:14px;line-height:1.85}.prose h3{font-size:21px;margin:34px 0 14px;letter-spacing:-.02em}.prose h4{font-size:17px;margin:26px 0 10px}.prose p{margin:14px 0}.prose li{margin:5px 0}.prose code{font:12px Consolas,"Cascadia Code",monospace;padding:2px 5px;border-radius:4px;background:#edf2f8;color:#264c7c;overflow-wrap:anywhere}.prose pre,.json-example pre{position:relative;background:#17263c;border-radius:8px;color:#d9e7fa;padding:21px;overflow:auto;line-height:1.7;tab-size:4}.prose pre code,.json-example pre code{background:none;color:inherit;padding:0;font:12px Consolas,monospace;white-space:pre;overflow-wrap:normal}.copy-code{position:absolute;right:8px;top:8px;background:#253951;color:#d2e1f4;border-color:#40526b;font-size:10px;padding:3px 8px}.table-scroll{overflow:auto;border:1px solid var(--line);border-radius:8px;margin:18px 0 23px}table{border-collapse:collapse;width:100%;text-align:left;font-size:12px;line-height:1.6}th,td{padding:11px 13px;border-bottom:1px solid #e6ebf2;vertical-align:top}th{background:#f0f4fa;color:#405574;font-size:11px;font-weight:650}tr:last-child td{border-bottom:0}tbody tr:nth-child(even){background:#fafbfd}td code{font-size:11px!important}.catalog{max-height:75vh}.catalog th{position:sticky;top:0;z-index:1}.catalog td:first-child{font:12px Consolas,monospace;color:#245a99}.catalog td{min-width:100px}.catalog td:last-child{min-width:260px}.catalog-search{display:block;max-width:540px;font-size:12px;font-weight:600}.catalog-search input{margin-top:7px;font-weight:400}.muted{color:var(--muted);font-size:12px}.json-example{border:1px solid var(--line);border-radius:8px;margin:14px 0;overflow:hidden}.json-example summary{display:flex;align-items:center;justify-content:space-between;gap:14px;padding:13px 17px;cursor:pointer;background:#f7f9fc}.json-example summary strong{font:600 13px Consolas,monospace}.json-example summary span{font-size:11px;color:var(--muted)}.json-example pre{margin:0;border-radius:0;max-height:570px}.footer{border-top:1px solid var(--line);padding-top:20px;color:var(--muted);font-size:12px}.no-results{padding:30px;background:white;border-radius:10px;border:1px solid var(--line)}dialog{padding:0;border:1px solid var(--line);border-radius:12px;width:96vw;max-width:1600px;height:94vh;background:white;color:var(--ink)}dialog::backdrop{background:#152339b8}.dialog-heading{display:flex;align-items:center;justify-content:space-between;padding:12px 20px;border-bottom:1px solid var(--line)}.dialog-content{height:calc(100% - 65px);overflow:auto}.dialog-content .diagram-scroll{max-height:none;padding:25px;min-height:100%}.dialog-content svg{min-width:1000px}.mobile-menu{display:none}
@media(min-width:1600px){.main{padding-left:65px;padding-right:65px}}
@media(max-width:1000px){.sidebar{width:230px}.main{margin-left:230px;padding:24px}.chapter{padding:24px}.facts{gap:9px}.fact{padding:14px}.fact strong{font-size:15px}.figure-tools>span{display:none}}
@media(max-width:740px){.sidebar{position:relative;width:100%;height:auto}.brand{padding:17px 20px}.brand strong{font-size:14px}.brand span{display:none}.nav-scroll{display:none;max-height:65vh}.sidebar.is-open .nav-scroll{display:block}.sidebar-foot{display:none}.mobile-menu{display:block;position:absolute;right:18px;top:14px;color:white;border-color:#53647d;background:transparent}.main{margin:0;padding:20px 15px}.topbar{align-items:flex-end;gap:8px}.topbar-actions{justify-content:flex-end}.topbar-actions button{font-size:11px;padding:6px}.search input{font-size:12px}.hero{padding-top:17px}.hero p{font-size:14px}.facts{grid-template-columns:1fr;margin-top:16px}.fact{padding:12px 16px}.fact p{font-size:12px}.fact strong{margin:0;font-size:15px}.chapter{padding:21px 18px}.chapter-heading h2{font-size:24px}.map-header h2{font-size:23px}.figure-tools{padding:8px}.diagram-scroll{padding:10px}.prose{font-size:13px}th,td{padding:9px}.diagram-tabs{gap:5px}.diagram-tabs button{font-size:11px}}
@media print{body{background:white;color:#111;font-size:11pt}.sidebar,.topbar,.search-status,.diagram-tabs,.figure-tools,.copy-code,.catalog-search,.reading-path,.footer{display:none!important}.main{margin:0;padding:0;max-width:none}.chapter{display:block!important;border:0;border-radius:0;padding:0;margin:25px 0;break-before:page}.diagram-panel{display:block!important;break-inside:avoid}.diagram-scroll{max-height:none;overflow:visible}.diagram-scroll svg{min-width:0!important;width:100%!important}.table-scroll,.catalog{overflow:visible;max-height:none}th{position:static!important}pre{white-space:pre-wrap!important;overflow-wrap:anywhere;background:#f3f5f8!important;color:#111!important}.prose pre code{white-space:pre-wrap}.hero h1{font-size:30pt}.facts{break-inside:avoid}a{color:#163a69}dialog{display:none!important}}
.overview{padding:22px 28px}.overview .hero{padding:10px 0 6px}.overview .hero h1{font-size:clamp(28px,3vw,42px);margin:9px 0 12px}.overview .hero p{font-size:15px;margin:0 0 15px}.overview .facts{margin:18px 0 24px}
@media(max-width:740px){.overview{padding:20px 18px}.overview .hero p{font-size:14px}}
"""


SCRIPT = r"""
(()=>{
  const all=(selector,root=document)=>[...root.querySelectorAll(selector)];
  const chapters=all('.chapter'),links=all('.nav-link'),search=document.getElementById('guide-search');
  const normalize=value=>value.toLocaleLowerCase('tr-TR');
  function status(message){document.getElementById('search-status').textContent=message;}
  function select(id,scroll=true){
    const target=document.getElementById(id)||document.getElementById('overview');
    const chapter=target.closest('.chapter')||target;
    links.forEach(link=>{const active=link.hash==='#'+chapter.id;link.classList.toggle('active',active);if(active)link.setAttribute('aria-current','location');else link.removeAttribute('aria-current');});
    if(scroll)requestAnimationFrame(()=>target.scrollIntoView({block:'start'}));
    document.querySelector('.sidebar').classList.remove('is-open');
    document.getElementById('mobile-menu').setAttribute('aria-expanded','false');
  }
  window.addEventListener('hashchange',()=>select(decodeURIComponent(location.hash.slice(1))));
  document.addEventListener('click',event=>{
    const anchor=event.target.closest('a[href^="#"]');
    if(anchor&&anchor.closest('dialog'))document.getElementById('diagram-dialog').close();
    if(anchor&&anchor.hash===location.hash){event.preventDefault();select(decodeURIComponent(anchor.hash.slice(1)));}
  });
  document.getElementById('print-guide').onclick=()=>{
    all('details.json-example').forEach(item=>item.open=true);window.print();
  };
  let results=[],resultIndex=-1;
  search.addEventListener('input',()=>{
    const query=normalize(search.value.trim());
    results=[];resultIndex=-1;
    chapters.forEach(chapter=>{const match=!!query&&normalize(chapter.textContent).includes(query);chapter.classList.toggle('search-match',match);if(match)results.push(chapter);});
    links.forEach(link=>link.classList.toggle('search-hit',results.includes(document.getElementById(link.hash.slice(1)))));
    document.getElementById('no-results').hidden=!query||results.length>0;
    document.getElementById('search-next').disabled=!results.length;
    status(query?results.length+' bölüm eşleşti. Enter ile sonraki sonuca git.':'Bütün bölümler aynı sayfada; menü ilgili başlığa kaydırır.');
  });
  function nextResult(){if(!results.length)return;resultIndex=(resultIndex+1)%results.length;const chapter=results[resultIndex];location.hash=chapter.id;chapter.scrollIntoView({block:'start'});status((resultIndex+1)+' / '+results.length+' · '+chapter.dataset.title);}
  search.addEventListener('keydown',event=>{if(event.key==='Enter'){event.preventDefault();nextResult();}});
  document.getElementById('search-next').onclick=nextResult;
  const field=document.getElementById('field-search'),rows=all('#field-table tbody tr');
  field.addEventListener('input',()=>{const query=normalize(field.value.trim());let count=0;rows.forEach(row=>{row.hidden=!normalize(row.textContent).includes(query);if(!row.hidden)count++;});document.getElementById('field-count').textContent=count+' / '+rows.length+' alan satırı';});
  const dialog=document.getElementById('diagram-dialog'),content=document.getElementById('dialog-content');
  let expanded=null,placeholder=null;
  document.getElementById('close-diagram').onclick=()=>dialog.close();
  dialog.addEventListener('close',()=>{if(expanded&&placeholder){placeholder.replaceWith(expanded);expanded=null;placeholder=null;}});
  document.addEventListener('click',event=>{
    const button=event.target.closest('[data-diagram]');if(!button)return;
    const scroll=button.closest('figure').querySelector('.diagram-scroll'),svg=scroll.querySelector('svg'),action=button.dataset.diagram;
    if(action==='expand'){
      expanded=scroll;placeholder=document.createElement('div');scroll.replaceWith(placeholder);content.append(scroll);dialog.showModal();return;
    }
    let zoom=Number(svg.dataset.zoom||1);zoom=action==='reset'?1:Math.max(.65,Math.min(3,zoom*(action==='in'?1.25:.8)));
    svg.dataset.zoom=zoom;svg.style.width=(zoom*100)+'%';
  });
  all('pre').forEach(pre=>{
    const button=document.createElement('button');button.type='button';button.className='copy-code';button.textContent='Kopyala';pre.append(button);
    button.onclick=async()=>{
      const text=pre.querySelector('code')?.textContent||'';
      try{
        if(navigator.clipboard?.writeText)await navigator.clipboard.writeText(text);
        else{const area=document.createElement('textarea');area.value=text;document.body.append(area);area.select();const copied=document.execCommand('copy');area.remove();if(!copied)throw Error();}
        button.textContent='Kopyalandı';
      }catch{button.textContent='Metni seçerek kopyala';}
      setTimeout(()=>button.textContent='Kopyala',1800);
    };
  });
  document.getElementById('mobile-menu').onclick=()=>{const open=document.querySelector('.sidebar').classList.toggle('is-open');document.getElementById('mobile-menu').setAttribute('aria-expanded',String(open));};
  select(decodeURIComponent(location.hash.slice(1))||'overview',!!location.hash);
  status('Bütün bölümler aynı sayfada; menü ilgili başlığa kaydırır.');
  if('IntersectionObserver' in window){const observer=new IntersectionObserver(entries=>{for(const entry of entries){if(entry.isIntersecting){const chapter=entry.target;links.forEach(link=>{const active=link.hash==='#'+chapter.id;link.classList.toggle('active',active);if(active)link.setAttribute('aria-current','location');else link.removeAttribute('aria-current');});}}},{rootMargin:'-5% 0px -80% 0px'});chapters.forEach(chapter=>observer.observe(chapter));}
})();
"""


def build():
    links = LinkRewriter()
    complete_uml, uml_stats = otopilot_uml.section(ROOT)
    sections = []
    for key, title, filename, _ in DOCUMENTS:
        output = render_document(key, title, filename, links)
        if key == "tcp":
            output = output.replace("</div></section>", message_examples() + "</div></section>")
        sections.append(output)
    sections.append(data_section())
    nav = ['<a class="nav-link" href="#otopilot-uml"><b>UML</b>Otopilot: bütün sınıflar</a>',
           '<a class="nav-link" href="#otopilot-members"><b>↳</b>Bütün üye değişkenler</a>',
           '<a class="nav-link" href="#overview"><b>⌘</b>Genel bakış</a>',
           '<a class="nav-link" href="#overview-maps"><b>↳</b>Özet bağlantı haritaları</a>']
    previous_group = None
    for index, (key, title, _, group) in enumerate(DOCUMENTS, 1):
        if group != previous_group:
            nav.append('<div class="nav-group">' + escape(group) + '</div>')
            previous_group = group
        nav.append(f'<a class="nav-link" href="#{key}"><b>{index:02}</b>{escape(title)}</a>')
    nav.append('<div class="nav-group">Referans</div><a class="nav-link" href="#alanlar"><b>Σ</b>Alan kataloğu</a>')
    tabs, panels = [], []
    for kind, label, title, description in DIAGRAMS:
        tabs.append(f'<a href="#map-{kind}">{escape(label)} ↓</a>')
        panels.append(
            f'<div class="diagram-panel" id="map-{kind}">'
            '<div class="diagram-description"><h3>' + escape(title) + '</h3><p>' + escape(description)
            + '</p></div>' + figure(kind, "map-" + kind) + '</div>')
    overview = (
        '<section class="chapter overview" id="overview" data-title="Bağlantı haritası">'
        '<header class="hero"><span class="eyebrow">BVR Fight Project / Mimari ve kullanım</span>'
        '<h1>Otopilot sınıfları ve<br>birbirleriyle bağlantıları.</h1>'
        '<p>Sim’den iki bağımsız otopilota, manevralardan uçak ve füze modeline uzanan yapıyı incele. '
        'Tam UML ve üye listeleri hemen aşağıda. Bütün bölümler bu sayfada açık kalır.</p>'
        '<div class="chips"><span class="chip">Çevrimdışı açılır</span>'
        f'<span class="chip">{uml_stats["types"]} C++ türü · bütün üye değişkenler</span>'
        '<span class="chip">Kaynak kodundan üretilen tam UML</span></div>'
        '<p><a href="#otopilot-uml">Tam otopilot UML diyagramına git ↓</a> · '
        '<a href="#otopilot-members">Sınıf ve değişken listesine git ↓</a></p></header>'
        '<div class="facts"><div class="fact green"><span>01 / Ortak durum</span>'
        '<strong>Sim → World → Aircraft</strong><p>Dünya iki uçağı; uçaklar profil, kinematik, füze ve hesaplanan düşman bilgisini taşır.</p></div>'
        '<div class="fact"><span>02 / Ayrı kararlar</span><strong>İki bağımsız otopilot</strong>'
        '<p>Fighter ve hostile kendi komutunu, karar durumunu ve manevra nesnelerini saklar.</p></div>'
        '<div class="fact violet"><span>03 / Uçuş hedefi</span><strong>Maneuver → FlightCommand</strong>'
        '<p>Hold, Out, In, Crank ve Notch ortak arayüzden uçuş hedefi üretir.</p></div></div>'
        '</section>' + complete_uml
        + '<section class="chapter" id="overview-maps" data-title="Özet bağlantı haritaları">'
        '<div class="map-header"><div><h2>Bağlantı haritası</h2>'
        '<p>Oklar veri veya çağrı yönünü; dolu elmaslar üye sahipliğini gösterir.</p></div></div>'
        '<p class="muted">Aşağıdakiler özet görünüşlerdir. Bütün üyeleri içeren harita <a href="#otopilot-uml">Tam otopilot UML</a> bölümündedir.</p>'
        '<div class="diagram-jumps" aria-label="Özet diyagramlara git">'
        + "".join(tabs) + '</div>' + "".join(panels)
        + '<aside class="reading-path"><h3>Önerilen okuma sırası</h3>'
        '<p>Önce süreçleri, sonra sınıf sahipliğini ve adım sırasını incele. '
        'Değişiklik yaparken ilgili dosya ve geliştirme rehberine geç.</p>'
        '<div class="reading-links"><a href="#sistem">Sistem mimarisi →</a>'
        '<a href="#cpp">C++ sınıfları →</a><a href="#gelistirme">Kodda değişiklik →</a>'
        '<a href="#alanlar">Bir verinin kaynağı →</a></div></aside></section>'
    )
    template = (
        '<!doctype html><html lang="tr"><head><meta charset="utf-8">'
        '<meta name="viewport" content="width=device-width,initial-scale=1">'
        '<meta name="color-scheme" content="light">'
        '<title>BVR Fight Project · Otopilot UML ve teknik rehber</title><style>' + CSS + otopilot_uml.CSS + '</style></head><body>'
        '<aside class="sidebar" aria-label="Rehber bölümleri"><div class="brand">'
        '<strong>BVR / FIGHT PROJECT</strong><span>Mimari · kullanım · veri sözleşmesi</span></div>'
        '<button type="button" class="mobile-menu" id="mobile-menu" aria-expanded="false" '
        'aria-controls="guide-nav">Bölümler ☰</button>'
        '<nav class="nav-scroll" id="guide-nav">' + "".join(nav) + '</nav>'
        '<div class="sidebar-foot">Tek dosya · internet gerektirmez<br>SVG diyagramlar HTML içinde</div></aside>'
        '<main class="main"><div class="topbar"><div class="search">'
        '<label for="guide-search">REHBERDE ARA</label><input type="search" id="guide-search" '
        'placeholder="World, füze, kayıt, birimler…" autocomplete="off"></div>'
        '<div class="topbar-actions"><button type="button" id="search-next" disabled>Sonraki sonuç</button>'
        '<button type="button" id="print-guide">Yazdır / PDF</button></div></div>'
        '<div id="search-status" class="search-status" role="status" aria-live="polite"></div>'
        '<p id="no-results" class="no-results" hidden>Bu arama için bölüm bulunamadı.</p>'
        + overview + "".join(sections)
        + '<footer class="footer">Kaynaklar: proje Markdown belgeleri, gerçek TCP örnekleri ve alan kataloğu. '
        'Yeniden üret: <code>python docs/build_html.py</code>. '
        '<a href="README.md">Belge dizini</a></footer></main>'
        '<dialog id="diagram-dialog" aria-label="Genişletilmiş diyagram">'
        '<div class="dialog-heading"><strong>Bağlantı diyagramı</strong>'
        '<button type="button" id="close-diagram" autofocus>Kapat · Esc</button></div>'
        '<div id="dialog-content" class="dialog-content"></div></dialog>'
        '<script>' + SCRIPT + otopilot_uml.SCRIPT + '</script></body></html>'
    )
    DESTINATION.write_text(template, encoding="utf-8")
    print(f"Built {DESTINATION.relative_to(ROOT)} ({len(template.encode('utf-8')):,} bytes)")
    print(f"{len(DOCUMENTS)} documents, complete C++ UML {uml_stats}, 7 overview diagrams; continuous page.")


if __name__ == "__main__":
    build()
