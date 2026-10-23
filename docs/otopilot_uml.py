"""Source inventory and complete UML for the project's own otopilot C++ types.

This is a deliberately small declaration reader for this project's C++17 style,
not a general C++ parser. Unknown member declarations fail the build rather than
silently disappearing from the documentation. No application code is executed.
"""
from __future__ import annotations

from dataclasses import dataclass, field
from html import escape
from pathlib import Path
import re
import textwrap


@dataclass
class Member:
    name: str
    type: str
    visibility: str
    initial: str = ""

    def label(self):
        return f"{self.visibility} {self.name}: {self.type}" + (f" = {self.initial}" if self.initial else "")


@dataclass
class Type:
    name: str
    kind: str
    source: str
    line: int
    body: str
    bases: list[str] = field(default_factory=list)
    members: list[Member] = field(default_factory=list)
    methods: list[str] = field(default_factory=list)
    values: list[str] = field(default_factory=list)
    implementation: str = ""

    @property
    def key(self):
        return "type-" + self.name.replace("::", "-")


@dataclass(frozen=True)
class Relation:
    source: str
    target: str
    kind: str
    label: str


def clean(text):
    """Remove comments while preserving offsets, strings and line numbers."""
    pattern = r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|//[^\n]*|/\*.*?\*/'
    return re.sub(pattern, lambda m: re.sub(r"[^\n]", " ", m[0]) if m[0].startswith(("//", "/*")) else m[0], text, flags=re.S)


def closing(text, start, left="{", right="}"):
    depth, quote, escaped = 0, None, False
    for pos in range(start, len(text)):
        char = text[pos]
        if quote:
            if escaped:
                escaped = False
            elif char == "\\":
                escaped = True
            elif char == quote:
                quote = None
            continue
        if char in "\"'":
            quote = char
        elif char == left:
            depth += 1
        elif char == right:
            depth -= 1
            if depth == 0:
                return pos
    raise ValueError("Unclosed declaration")


def compact(value):
    return re.sub(r"\s+", " ", value).strip()


def declarations(body):
    """Yield direct members, ignoring locals inside inline method bodies."""
    start, pos = 0, 0
    while pos < len(body):
        access = re.match(r"\s*(public|protected|private)\s*:", body[pos:]) if pos == start else None
        if access:
            yield "access", access[1]
            pos += access.end()
            start = pos
            continue
        char = body[pos]
        if char in "\"'":
            # Strings can occur in default expressions.
            match = re.match(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'', body[pos:])
            if match:
                pos += match.end()
                continue
        if char == "(":
            pos = closing(body, pos, "(", ")") + 1
            continue
        if char == "{":
            end = closing(body, pos)
            prefix = body[start:pos]
            if "(" in prefix:
                # A constructor may have a member initializer list. Preserve its
                # signature and skip its body, not the following member.
                yield "method", compact(prefix)
                pos = end + 1
                start = pos
                continue
            pos = end + 1
            continue
        if char == ";":
            value = compact(body[start:pos])
            if value:
                yield ("method" if "(" in value else "declaration"), value
            pos += 1
            start = pos
            continue
        pos += 1
    if body[start:].strip():
        raise ValueError("Unparsed member: " + body[start:])


def inventory(root):
    base = root / "src/otopilot"
    paths = sorted(p for p in base.rglob("*") if p.suffix in {".hpp", ".cpp"}
                   and not {"third_party", "build"}.intersection(p.relative_to(base).parts))
    types, sources = [], {}
    definition = re.compile(r"\b(enum\s+class|struct|class)\s+([A-Za-z_]\w*(?:::\w+)*)\s*(?::\s*([^;{]+))?\s*\{")
    for path in paths:
        text = clean(path.read_text(encoding="utf-8"))
        sources[path] = text
        pos = 0
        while match := definition.search(text, pos):
            end = closing(text, match.end()-1)
            kind, name, bases = match.groups()
            item = Type(name, compact(kind), path.relative_to(root).as_posix(),
                        text.count("\n", 0, match.start())+1, text[match.end():end])
            item.bases = re.findall(r"(?:public|protected|private)\s+(\w+)", bases or "")
            if kind.startswith("enum"):
                item.values = [compact(value) for value in item.body.split(",") if value.strip()]
            else:
                visibility = "+" if kind == "struct" else "−"
                for category, value in declarations(item.body):
                    if category == "access":
                        visibility = {"public": "+", "private": "−", "protected": "#"}[value]
                    elif category == "method":
                        # Drop constructor initializer list, retaining default/
                        # delete/pure virtual/const/override qualifiers.
                        value = re.sub(r"\)\s*:(?!:)\s*.*$", ")", value)
                        item.methods.append(visibility + " " + value)
                    elif re.fullmatch(r"(?:struct|class)\s+\w+", value):
                        continue  # forward declaration, not a data member
                    else:
                        field_match = re.fullmatch(r"(.+?[\s*&])([A-Za-z_]\w*)\s*(?:(=\s*.*|\{.*\}))?", value)
                        if not field_match:
                            raise ValueError(f"Unsupported member {name}: {value}")
                        field_type, field_name, initial = field_match.groups()
                        item.members.append(Member(field_name, compact(field_type), visibility,
                                                   (initial or "").lstrip("= ")))
            types.append(item)
            pos = end + 1
    names = [item.name for item in types]
    if len(names) != len(set(names)):
        raise ValueError("Duplicate type definitions")
    for item in types:
        # Include bodies of this type's out-of-line methods. A one-type source
        # file also has file-local helpers (notably World.update's calculations).
        counterpart = root / item.source.replace(".hpp", ".cpp") if item.source.endswith(".hpp") else None
        same_file_types = [other for other in types if other.source == item.source]
        if counterpart in sources and len(same_file_types) == 1:
            item.implementation = sources[counterpart]
        elif counterpart in sources:
            code = sources[counterpart]
            for method in re.finditer(r"\b" + re.escape(item.name) + r"::(?:~?\w+|operator=)\s*\(", code):
                paren_end = closing(code, method.end()-1, "(", ")")
                brace = code.find("{", paren_end)
                semi = code.find(";", paren_end)
                if brace >= 0 and (semi < 0 or brace < semi):
                    item.implementation += code[method.start():closing(code, brace)+1]
    return types, relationships(types)


def references(text, names):
    return {name for name in names if re.search(r"(?<![\w:])" + re.escape(name) + r"(?!\w)", text)}


def relationships(types):
    names = {item.name for item in types}
    enum_names = {item.name for item in types if item.values}
    relations = []
    for item in types:
        if item.values:
            continue  # Phase::In is an enum literal, not a reference to class In
        member_targets = set()
        for member in item.members:
            resolved = member.type.replace("<Impl>", "<Connection::Impl>") if item.name == "Connection" else member.type
            for target in sorted(references(resolved, names) - {item.name}):
                kind = "association" if "*" in resolved or "&" in resolved else "composition"
                if "unique_ptr" in resolved:
                    kind = "composition"
                if target in enum_names:
                    kind = "enum"
                multiplicity = "0..*" if "vector<" in resolved else "1"
                if "unique_ptr" in resolved:
                    multiplicity = "0..1"
                relations.append(Relation(item.name, target, kind, f"{member.name} [{multiplicity}]"))
                member_targets.add(target)
        for base in item.bases:
            relations.append(Relation(item.name, base, "inheritance", "kalıtım"))
        uses = references(item.body + "\n" + item.implementation, names) - member_targets - set(item.bases) - {item.name}
        for target in sorted(uses):
            relations.append(Relation(item.name, target, "dependency", "tür / hesap kullanımı"))
    # The target is represented by an ID, not an owning pointer. The condition
    # is implemented in world's read_missile_info; never draw this as ownership.
    if {"Missile", "Aircraft"} <= names:
        relations.append(Relation("Missile", "Aircraft", "association", "target_id = Aircraft.id (World eşleştirir)"))
    return relations


LANES = [
    ("Adım ve bağımsız otopilotlar", ["Sim", "OtopilotFighter", "OtopilotHostile", "World", "FlightCommand"]),
    ("Manevralar ve kalıtım", ["Maneuvers", "Maneuver", "Hold", "Out", "In", "Crank", "Notch", "CrankSide"]),
    ("Uçak ve hesaplanan durum", ["Aircraft", "Kinematics", "EnemyInfo", "Wez", "Phase"]),
    ("Profil ve füze modeli", ["AircraftProfile", "AltitudeFactor", "Missile", "MissileProfile", "MissileTargetInfo", "MissileType", "MissileState", "MissileOutcome"]),
    ("Geometri ve taktik hesaplar", ["Geometry", "Engagement", "MissileGeometry", "Ned", "Lla", "Ecef"]),
    ("Otopilot giriş / çıkış sınırı", ["StateReader", "CommandWriter", "CalculationWriter", "ConsoleLog", "Message", "Connection", "Connection::Impl"]),
]


def row_lines(text):
    return textwrap.wrap(text, width=94, break_long_words=False, break_on_hyphens=False) or [""]


def box_rows(item):
    attributes = item.values if item.kind.startswith("enum") else [m.label() for m in item.members]
    attributes = attributes or ["(üye değişken yok)"]
    methods = item.methods or ([] if item.values else ["(tanımlı metot yok)"])
    return ([line for value in attributes for line in row_lines(value)],
            [line for value in methods for line in row_lines(value)])


def complete_svg(types, relations):
    by_name = {item.name: item for item in types}
    listed = [name for _, names in LANES for name in names]
    if set(listed) != set(by_name) or len(listed) != len(set(listed)):
        raise ValueError(f"Update UML lanes for types: {set(by_name)^set(listed)}")
    width, gap, margin = 810, 190, 45
    layout, headers = {}, []
    for column, (title, names) in enumerate(LANES):
        x, y = margin + column*(width+gap), 145
        headers.append(f'<text x="{x}" y="105" class="uml-lane">{escape(title)}</text>')
        for name in names:
            attributes, methods = box_rows(by_name[name])
            height = 102 + 20*(len(attributes)+len(methods)) + (20 if methods else 0)
            layout[name] = (x, y, width, height, column)
            y += height + 70
    total_width = margin*2 + len(LANES)*width + (len(LANES)-1)*gap
    total_height = max(y+h for x, y, w, h, col in layout.values())+60
    svg = [f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {total_width} {total_height}" '
           f'role="img" aria-labelledby="full-uml-title" class="complete-uml" data-width="{total_width}">',
           '<title id="full-uml-title">Otopilot: bütün sınıflar, üye değişkenler, metotlar ve ilişkiler</title>',
           '<defs><marker id="full-arrow" viewBox="0 0 12 12" refX="11" refY="6" markerWidth="12" markerHeight="12" orient="auto" markerUnits="userSpaceOnUse"><path d="M1 1 L11 6 L1 11" fill="none" stroke="#64758c" stroke-width="1.6"/></marker>',
           '<marker id="full-triangle" viewBox="0 0 16 16" refX="15" refY="8" markerWidth="16" markerHeight="16" orient="auto" markerUnits="userSpaceOnUse"><path d="M1 1 L15 8 L1 15 Z" fill="white" stroke="#8052af" stroke-width="1.5"/></marker>',
           '<marker id="full-diamond" viewBox="0 0 18 12" refX="1" refY="6" markerWidth="18" markerHeight="12" orient="auto" markerUnits="userSpaceOnUse"><path d="M1 6 L9 1 L17 6 L9 11 Z" fill="#246b59"/></marker></defs>',
           '<rect width="100%" height="100%" fill="#fbfcfe"/>', *headers]
    # Assign each edge its own gutter. Nodes cover crossing routes; selecting a
    # type highlights only its direct relations without removing other classes.
    for index, relation in enumerate(relations):
        ax, ay, aw, ah, ac = layout[relation.source]
        bx, by, bw, bh, bc = layout[relation.target]
        offset = 25 + (index % 17)*7
        start_y = ay + 40 + (index % max(1, (ah-65)//18))*18
        end_y = by + 40 + (index % max(1, (bh-65)//18))*18
        if ac == bc:
            sx, tx, gutter = ax+aw, bx+bw, ax+aw+offset
            path = f"M {sx} {start_y} H {gutter} V {end_y} H {tx}"
        else:
            sx, tx = (ax+aw, bx) if ac < bc else (ax, bx+bw)
            sg, tg = sx+(offset if ac < bc else -offset), tx+(-offset if ac < bc else offset)
            rail = 120 - (index % 15)*5
            path = f"M {sx} {start_y} H {sg} V {rail} H {tg} V {end_y} H {tx}"
        marker = ('marker-start="url(#full-diamond)"' if relation.kind == "composition" else
                  'marker-end="url(#full-triangle)"' if relation.kind == "inheritance" else 'marker-end="url(#full-arrow)"')
        svg.append(f'<g class="uml-edge {relation.kind}" data-from="{escape(relation.source)}" data-to="{escape(relation.target)}">'
                   f'<title>{escape(relation.source)} → {escape(relation.target)}: {escape(relation.label)}</title>'
                   f'<path d="{path}" {marker}/></g>')
    for item in types:
        x, y, w, h, col = layout[item.name]
        attributes, methods = box_rows(item)
        svg.append(f'<a href="#{item.key}" class="uml-type" data-type="{escape(item.name)}">'
                   f'<g class="uml-box lane-{col}" data-x="{x}" data-y="{y}" data-w="{w}" data-h="{h}">'
                   f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="5"/>'
                   f'<text x="{x+18}" y="{y+22}" class="uml-kind">«{escape(item.kind)}» · {escape(item.source)}</text>'
                   f'<text x="{x+18}" y="{y+50}" class="uml-name">{escape(item.name)}</text>'
                   f'<path class="uml-rule" d="M{x} {y+64} H{x+w}"/>')
        for row, value in enumerate(attributes):
            svg.append(f'<text x="{x+18}" y="{y+89+row*20}" class="uml-member">{escape(value)}</text>')
        if methods:
            top = y + 99 + len(attributes)*20
            svg.append(f'<path class="uml-rule" d="M{x} {top-10} H{x+w}"/>')
            for row, value in enumerate(methods):
                svg.append(f'<text x="{x+18}" y="{top+13+row*20}" class="uml-member">{escape(value)}</text>')
        svg.append('</g></a>')
    return "".join(svg) + "</svg>"


def section(root):
    types, relations = inventory(root)
    members = sum(len(item.members) for item in types)
    methods = sum(len(item.methods) for item in types)
    enums = sum(bool(item.values) for item in types)
    options = ''.join(f'<option value="{escape(item.name)}">{escape(item.name)}</option>' for item in sorted(types, key=lambda t:t.name))
    result = [
        '<section class="chapter" id="otopilot-uml" data-title="Otopilot tam sınıf UML">'
        '<div class="chapter-heading"><span class="eyebrow">C++ / Tam sınıf envanteri</span>'
        '<h2>Otopilot: bütün sınıflar ve değişkenleri</h2>'
        '<p>Her kutunun üstünde sınıf adı, ortasında bütün üye değişkenler ve tipleri, altında metot imzaları bulunur. '
        'Manevralar kalıtımla; World, uçak ve profiller üye sahipliğiyle bağlıdır. '
        'Fighter ve hostile aynı sınıf tanımlarını kullanır, ayrı nesnelere ve ayrı duruma sahiptir.</p>'
        f'<p class="uml-stats">{len(types)-enums} sınıf / struct · {enums} enum · {members} üye değişken · '
        f'{methods} metot bildirimi · {len(relations)} ilişki</p></div>'
        '<p><strong>Kapsam:</strong> src/otopilot içindeki proje sınıfları; .cpp içinde tanımlanan Connection::Impl dahil. '
        'Python, arayüz, standart kütüphane ve üçüncü taraf sınıflar bu envantere dahil değildir. '
        'Yerel fonksiyon değişkenleri sınıf üyesi değildir. Json bir nlohmann::json takma adıdır.</p>'
        '<p class="uml-legend"><span>◆— üye sahipliği</span> <span>—▷ kalıtım</span> '
        '<span>⇢ tür / hesap kullanımı</span> <span>→ kimlikle ilişki</span> '
        '<span>+ public · − private · # protected</span> <span>[1] tek · [0..*] liste</span></p>'
        '<p class="muted">Lla ve Ecef, util_functions.hpp içindeki serbest dönüşüm fonksiyonlarının veri tipleridir. '
        'Mevcut sınıflar bu dönüşümleri çağırmadığı için haritada bağımsız görünürler; Geometry şu anda düz dünya hesabını kullanır. '
        'Kesik çizgiler enum alan tiplerini de gösterir; enum değeri sınıf sahipliği değildir.</p>'
        '<figure class="diagram-figure full-figure"><div class="uml-controls">'
        '<label for="uml-select">Sınıfa odaklan</label><select id="uml-select"><option value="">Bütün sınıflar</option>' + options + '</select>'
        '<button type="button" id="uml-focus">Yakınlaş</button><button type="button" id="uml-fit">Bütünü göster</button>'
        '<button type="button" id="uml-out" aria-label="Tam UML küçült">−</button>'
        '<button type="button" id="uml-in" aria-label="Tam UML büyüt">+</button>'
        '<a href="#otopilot-members">Üye listelerine git ↓</a></div>'
        '<p class="uml-help" id="uml-help" aria-live="polite">Büyük haritayı kaydırarak gez. Sınıf seçip Yakınlaş ile bütün üyelerini oku; kutuya tıklayınca aşağıdaki açıklamasına gidersin.</p>'
        '<div class="uml-viewport" tabindex="0" aria-label="Kaydırılabilir tam otopilot UML diyagramı">',
        complete_svg(types, relations), '</div><figcaption>Seçilen sınıfın doğrudan bağlantıları koyulaşır; '
        'diğer sınıflar haritada kalır. Okların kesin anlamı ve üye adları aşağıdaki ilişki listelerinde yer alır. '
        'Envanter ve sahiplik bağlantıları C++ kaynaklarından üretilir.</figcaption></figure>',
        '<div id="otopilot-members"><h3>Sınıflar, bütün üyeler ve bağlantıları</h3>'
        '<p>Bir sınıfın üye değişkeni yoksa açıkça belirtilir. Enum kutuları bütün seçeneklerini gösterir. '
        'Başlangıç değeri yazılmayan alanlarda kaynakta açık bir başlangıç ifadesi yoktur. '
        'Metotların yerel değişkenleri ve kalıtımla gelen üyeler tekrar listelenmez; kalıtım bağlantısından üst sınıfa geçilir.</p></div>',
        '<div class="uml-index">' + ''.join(f'<a href="#{item.key}">{escape(item.name)}</a>' for item in types) + '</div>',
    ]
    kind_names = {"composition": "Üye sahipliği", "association": "İlişki", "inheritance": "Kalıtım", "dependency": "Kullanım", "enum": "Enum alan tipi"}
    for title, names in LANES:
        result.append('<h3>' + escape(title) + '</h3>')
        for name in names:
            item = next(t for t in types if t.name == name)
            source_link = "../" + item.source
            result.append(f'<article class="type-card" id="{item.key}" data-class="{escape(item.name)}">'
                          f'<header><div><span class="eyebrow">{escape(item.kind)}</span><h4>{escape(item.name)}</h4></div>'
                          f'<button type="button" data-uml-focus="{escape(item.name)}">Diyagramda gör ↑</button></header>'
                          f'<p class="muted">Kaynak: <a href="{source_link}">{escape(item.source)}</a> · satır {item.line}</p>')
            if item.values:
                result.append('<p><strong>Enum değerleri</strong></p><ul>' + ''.join('<li><code>'+escape(v)+'</code></li>' for v in item.values)+'</ul>')
            elif item.members:
                result.append('<div class="table-scroll"><table><thead><tr><th>Erişim</th><th>Değişken</th><th>Tip</th><th>Başlangıç ifadesi</th></tr></thead><tbody>')
                for member in item.members:
                    result.append('<tr>'+''.join('<td><code>'+escape(v)+'</code></td>' for v in (member.visibility, member.name, member.type, member.initial or '—'))+'</tr>')
                result.append('</tbody></table></div>')
            else:
                result.append('<p class="muted">Üye değişken yok.</p>')
            if item.methods:
                result.append('<p><strong>Metotlar</strong></p><ul class="uml-methods">'+''.join('<li><code>'+escape(m)+'</code></li>' for m in item.methods)+'</ul>')
            related = [r for r in relations if item.name in (r.source, r.target)]
            if related:
                result.append('<p><strong>Bağlantılar</strong></p><ul class="uml-relations">')
                for relation in related:
                    source = next(t for t in types if t.name == relation.source)
                    target = next(t for t in types if t.name == relation.target)
                    result.append(f'<li><a href="#{source.key}">{escape(source.name)}</a> → '
                                  f'<a href="#{target.key}">{escape(target.name)}</a> · '
                                  f'{kind_names[relation.kind]}: {escape(relation.label)}</li>')
                result.append('</ul>')
            result.append('</article>')
    result.append('</section>')
    return ''.join(result), {"types": len(types), "enums": enums, "members": members, "methods": methods, "relations": len(relations)}


CSS = r"""
.uml-stats{font-weight:650;color:#235bc1!important}.uml-legend{display:flex;gap:12px 22px;flex-wrap:wrap;font-size:12px;color:#526783}.uml-controls{display:flex;align-items:center;gap:8px;flex-wrap:wrap;background:#f0f4fa;padding:14px}.uml-controls label{font-size:12px;font-weight:600}.uml-controls select{font:13px Consolas,monospace;padding:8px;border:1px solid #c2cfdf;border-radius:6px;max-width:100%}.uml-controls button,.uml-controls a{font-size:12px}.uml-help{padding:8px 14px;margin:0;font-size:12px;color:#526783;border-bottom:1px solid #dce3eb}.uml-viewport{height:720px;max-height:78vh;overflow:auto;background:#fbfcfe;position:relative;overscroll-behavior:contain}.uml-viewport svg{display:block;width:100%;height:auto;max-width:none;min-width:0}.uml-lane{font:700 20px 'Segoe UI',sans-serif;fill:#21334c}.uml-box rect{fill:#f4f8ff;stroke:#7595c3;stroke-width:1.6}.uml-box.lane-1 rect{fill:#f8f4ff;stroke:#a38ac8}.uml-box.lane-2 rect,.uml-box.lane-3 rect{fill:#f0faf6;stroke:#68a994}.uml-box.lane-4 rect{fill:#fffbf1;stroke:#d5b16f}.uml-box.lane-5 rect{fill:#f5f7fa;stroke:#9badc2}.uml-rule{stroke:#c5d3e3;stroke-width:1}.uml-kind{font:12px 'Segoe UI',sans-serif;fill:#617187}.uml-name{font:700 23px 'Segoe UI',sans-serif;fill:#21334c}.uml-member{font:13px Consolas,monospace;fill:#243750}.uml-type{text-decoration:none}.uml-type:hover rect,.uml-type.selected rect{stroke:#235bc1;stroke-width:4}.uml-edge path{fill:none;stroke:#64758c;stroke-width:1.6}.uml-edge.composition path{stroke:#246b59}.uml-edge.dependency path{stroke-dasharray:7 5}.uml-edge.inheritance path{stroke:#8052af;stroke-width:2}.complete-uml.has-selection .uml-edge{opacity:.12}.complete-uml.has-selection .uml-edge.connected{opacity:1}.complete-uml.has-selection .uml-edge.connected path{stroke-width:3}.type-card{padding:22px;border:1px solid #dce3eb;border-radius:9px;margin:20px 0 28px;scroll-margin-top:25px;background:#fff}.type-card header{display:flex;justify-content:space-between;gap:10px;align-items:center}.type-card h4{font:650 22px 'Segoe UI',sans-serif;margin:4px 0}.type-card p{font-size:13px}.type-card code{font:12px Consolas,monospace;overflow-wrap:anywhere}.type-card th,.type-card td{font-size:12px}.type-card ul{font-size:12px;line-height:1.8;padding-left:22px}.uml-index{display:flex;gap:6px;flex-wrap:wrap;margin:20px 0}.uml-index a{font:12px Consolas,monospace;padding:5px 9px;border:1px solid #dce3eb;border-radius:5px;text-decoration:none}.search-match{outline:2px solid #d5a84a;outline-offset:2px}.search-hit{background:#fff0bb!important;color:#182941}.chapter,.diagram-panel{scroll-margin-top:25px}.diagram-jumps{display:flex;gap:7px;flex-wrap:wrap;margin:15px 0}.diagram-jumps a{padding:5px 10px;border:1px solid #dce3eb;border-radius:6px;font-size:12px;text-decoration:none}.diagram-panel{padding-top:20px;border-top:1px solid #e0e6ee;margin-top:25px}.type-card:target{border-color:#235bc1;box-shadow:0 0 0 2px #235bc122}.uml-viewport:focus-visible{outline:3px solid #5e9bef;outline-offset:-3px}
.uml-edge.enum path{stroke-dasharray:7 5}
.full-figure,.uml-viewport{overflow-anchor:none}
@media(max-width:740px){.uml-controls{padding:10px}.uml-controls select{width:100%}.type-card{padding:15px}.type-card header{align-items:flex-start;flex-direction:column}.uml-viewport{height:520px}.uml-lane{font-size:20px}}
@media print{.uml-controls,.uml-help,.type-card button{display:none}.uml-viewport{height:auto;max-height:none;overflow:visible}.uml-viewport svg{width:100%!important}.type-card{break-inside:avoid}.diagram-panel{break-inside:auto}.uml-index{display:none}}
"""


SCRIPT = r"""
(()=>{
 const svg=document.querySelector('.complete-uml'),viewport=document.querySelector('.uml-viewport'),select=document.getElementById('uml-select');
 const types=[...svg.querySelectorAll('[data-type]')],edges=[...svg.querySelectorAll('.uml-edge')];
 const width=Number(svg.dataset.width);let scale=0;
 function highlight(name){svg.classList.toggle('has-selection',!!name);types.forEach(t=>t.classList.toggle('selected',t.dataset.type===name));edges.forEach(e=>e.classList.toggle('connected',e.dataset.from===name||e.dataset.to===name));}
 function resize(value){scale=Math.max(.08,Math.min(2,value));svg.style.width=(width*scale)+'px';}
 function fit(){svg.style.width='100%';scale=viewport.clientWidth/width;viewport.scrollTo(0,0);select.value='';highlight('');document.getElementById('uml-help').textContent='Bütün sınıflar aynı haritada. Üyeleri okumak için sınıf seçip Yakınlaş düğmesine bas.';}
 function focus(name,scroll=true){
  const node=types.find(t=>t.dataset.type===name);if(!node){fit();return;}
  select.value=name;highlight(name);const box=node.querySelector('.uml-box');
  resize(Math.min(1.25,Math.max(.7,(viewport.clientWidth-60)/Number(box.dataset.w))));
  requestAnimationFrame(()=>{viewport.scrollTo({left:Number(box.dataset.x)*scale-25,top:Number(box.dataset.y)*scale-35,behavior:'instant'});if(scroll)document.querySelector('.full-figure').scrollIntoView({block:'start',behavior:'instant'});});
  const count=edges.filter(e=>e.dataset.from===name||e.dataset.to===name).length;
  document.getElementById('uml-help').textContent=name+' · '+count+' doğrudan ilişki. Kutuya tıklayınca tüm üyelerine ve bağlantı listesine gidersin.';
 }
 document.getElementById('uml-focus').onclick=()=>focus(select.value,false);
 select.onchange=()=>focus(select.value,false);
 document.getElementById('uml-fit').onclick=fit;
 function zoom(factor){const old=scale||viewport.clientWidth/width,cx=(viewport.scrollLeft+viewport.clientWidth/2)/old,cy=(viewport.scrollTop+viewport.clientHeight/2)/old;resize(old*factor);viewport.scrollTo(cx*scale-viewport.clientWidth/2,cy*scale-viewport.clientHeight/2);}
 document.getElementById('uml-in').onclick=()=>zoom(1.3);document.getElementById('uml-out').onclick=()=>zoom(1/1.3);
 document.querySelectorAll('[data-uml-focus]').forEach(button=>button.onclick=()=>focus(button.dataset.umlFocus));
 fit();
})();
"""
