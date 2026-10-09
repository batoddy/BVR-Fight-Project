"""Read BVRGym's native Tacview CSV exports without changing their files."""
import csv
import hashlib
import math
from pathlib import Path
import re
import threading
import time
import uuid
from telemetry import estimate_motion


class EmptyRecording(ValueError):
    """The logger has not saved any measurements yet."""


def fingerprint(directory):
    return tuple((p.name, p.stat().st_size, p.stat().st_mtime_ns)
                 for p in sorted(directory.glob('*.csv')) if p.is_file())


def metadata(path):
    # Tacview stores model, optional callsign and optional color in the filename.
    match = re.fullmatch(r'(.+?)(?: \(([^()]*)\))?(?: \[([^\[\]]+)\])?', path.stem)
    if match is None:
        raise ValueError(f'Geçersiz kayıt dosya adı: {path.name}')
    model, callsign, color = match.groups()
    color = (color or '').strip()
    team = color.lower() if color.lower() in ('blue', 'red') else 'unknown'
    upper = model.upper()
    missile = re.match(r'^(AIM|AAM|AMRAAM|R[- ]?\d|PL[- ]?\d|SD[- ]?\d|METEOR|MICA|IRIS|PYTHON|DERBY|ASPIDE)', upper)
    kind = 'missile' if missile else 'aircraft'
    visual_model = 'f16' if re.match(r'^F[- ]?16(?:\b|[A-Z])', upper) else 'aim120' if upper.startswith(('AIM-120', 'AMRAAM')) else 'generic'
    return dict(id=path.name, name=callsign or model, model=model, color=color,
                team=team, kind=kind, visual_model=visual_model, file=path.name)


def ecef(lat_gc_deg, lon_deg, height_m):
    """BVRGym native geocentric latitude and JSBSim radial ASL -> ECEF."""
    lat, lon = math.radians(lat_gc_deg), math.radians(lon_deg)
    a, b = 6378137.0, 6356752.314245
    sea_radius = 1 / math.sqrt((math.cos(lat) / a) ** 2 + (math.sin(lat) / b) ** 2)
    radius = sea_radius + height_m
    return (radius * math.cos(lat) * math.cos(lon),
            radius * math.cos(lat) * math.sin(lon), radius * math.sin(lat))


def geodetic_latitude(lat_gc_deg, height_m):
    x, _, z = ecef(lat_gc_deg, 0, height_m)
    e2 = 6.69437999014e-3
    phi = math.atan2(z, (1 - e2) * x)
    for _ in range(8):
        n = 6378137.0 / math.sqrt(1 - e2 * math.sin(phi) ** 2)
        phi = math.atan2(z + e2 * n * math.sin(phi), x)
    return math.degrees(phi)


def read_csv(path, handle):
    info = metadata(path)
    rows = []
    previous = {}
    fields = {'Time': 'sim_time_s', 'Longitude': 'longitude_deg',
              'Latitude': 'latitude_deg', 'Altitude': 'altitude_m',
              'Roll (deg)': 'roll_deg', 'Pitch (deg)': 'pitch_deg',
              'Yaw (deg)': 'heading_deg', 'TAS': 'true_airspeed_mps', 'Mach': 'mach'}
    reader = csv.DictReader(handle)
    # Native pandas exports unused ammunition as an empty file / blank line.
    if not reader.fieldnames:
        return []
    reader.fieldnames = [key.strip() for key in reader.fieldnames]
    if len(set(reader.fieldnames)) != len(reader.fieldnames):
        raise ValueError(f'{path.name}: yinelenen sütun adı')
    if not {'Time', 'Longitude', 'Latitude', 'Altitude'} <= set(reader.fieldnames):
        raise ValueError(f'{path.name}: Time, Longitude, Latitude, Altitude sütunları gerekli')
    for key in ('Roll', 'Pitch', 'Yaw'):
        if key in reader.fieldnames and key + ' (deg)' not in reader.fieldnames:
            fields[key] = fields.pop(key + ' (deg)')
    for row in reader:
        if not any(value for value in row.values()):
            continue
        try:
            if None in row:
                raise ValueError('sütun sayısı uyuşmuyor')
            values = dict(previous)
            for column, key in fields.items():
                raw = row.get(column)
                if raw is not None and raw.strip():
                    value = float(raw)
                    if not math.isfinite(value):
                        raise ValueError('sonlu olmayan sayı')
                    values[key] = value
            if not (row.get('Time') or '').strip():
                raise ValueError('zaman eksik')
            if not all(key in values for key in ('longitude_deg', 'latitude_deg', 'altitude_m')):
                raise ValueError('başlangıç konumu eksik')
            if not -90 <= values['latitude_deg'] <= 90 or not -180 <= values['longitude_deg'] <= 180:
                raise ValueError('koordinat aralık dışında')
            if values['sim_time_s'] < 0 or rows and values['sim_time_s'] < rows[-1]['sim_time_s']:
                raise ValueError('zaman negatif veya geriye gidiyor')
            if any(values.get(key, 0) < 0 for key in ('true_airspeed_mps', 'mach')):
                raise ValueError('hız veya Mach negatif olamaz')
            previous = dict(values)
            for key in ('heading_deg', 'pitch_deg', 'roll_deg'):
                values[key + '_known'] = key in previous
            for key in ('roll_deg', 'pitch_deg', 'heading_deg'):
                values.setdefault(key, 0.0)
            for key in ('true_airspeed_mps', 'mach'):
                values.setdefault(key, None)
            sample = dict(info, **values)
            if rows and rows[-1]['sim_time_s'] == sample['sim_time_s']:
                rows[-1] = sample
            else:
                rows.append(sample)
        except (TypeError, ValueError) as exc:
            raise ValueError(f'{path.name}, satır {reader.line_num}: {exc}') from exc
    if rows:
        start, end = rows[0]['sim_time_s'], rows[-1]['sim_time_s']
        for row in rows:
            row.update(record_start_s=start, record_end_s=end)
    return rows


def read_object(path):
    with path.open(encoding='utf-8-sig', newline='') as handle:
        return read_csv(path, handle)


def read_recording(directory):
    directory = Path(directory)
    paths = [path for path in sorted(directory.glob('*.csv')) if path.is_file()]
    return assemble([read_object(path) for path in paths], [metadata(path) for path in paths])


def assemble(objects, manifest=()):
    objects = [rows for rows in objects if rows]
    if not objects:
        raise EmptyRecording('BVRGym Tacview CSV kaydı henüz oluşmadı')
    for rows in objects:
        estimate_motion(rows, ecef)
    samples = sorted((row for rows in objects for row in rows), key=lambda row: row['sim_time_s'])
    first = next((row for row in samples if row['kind'] == 'aircraft'), samples[0])
    lat0 = math.radians(geodetic_latitude(first['latitude_deg'], first['altitude_m']))
    lon0 = math.radians(first['longitude_deg'])
    origin = ecef(first['latitude_deg'], first['longitude_deg'], first['altitude_m'])
    for index, row in enumerate(samples, 1):
        xyz = ecef(row['latitude_deg'], row['longitude_deg'], row['altitude_m'])
        dx, dy, dz = (xyz[i] - origin[i] for i in range(3))
        row['east_m'] = -math.sin(lon0) * dx + math.cos(lon0) * dy
        row['north_m'] = (-math.sin(lat0) * math.cos(lon0) * dx
                          - math.sin(lat0) * math.sin(lon0) * dy + math.cos(lat0) * dz)
        row['sequence'] = index
    entities = [dict(rows[0]) for rows in objects]
    aircraft = [entry for entry in entities if entry['kind'] == 'aircraft']
    tracks = {rows[0]['id']: rows for rows in objects}
    inventories = {entry['id']: [] for entry in aircraft}
    unassigned = []
    for info in manifest:
        if info['kind'] != 'missile':
            continue
        # Native filenames encode team, but no launcher ID. Never guess among teammates.
        candidates = [entry for entry in aircraft if info['color'] and entry['color'].casefold() == info['color'].casefold()]
        owner = candidates[0]['id'] if len(candidates) == 1 else None
        track = tracks.get(info['id'])
        munition = dict(info, owner_id=owner, ownership_source='team' if owner else None,
                        record_start_s=track[0]['record_start_s'] if track else None,
                        record_end_s=track[-1]['record_end_s'] if track else None,
                        has_track=bool(track))
        (inventories[owner] if owner else unassigned).append(munition)
        if track:
            for sample in track:
                sample['owner_id'] = owner
    for entry in aircraft:
        entry['inventory'] = inventories[entry['id']]
    # Kept once in entity metadata, rather than duplicated in every flight sample.
    if entities:
        entities[0]['unassigned_inventory'] = unassigned
    return samples, entities


def recording_warnings(entities):
    warnings = []
    aircraft = [e for e in entities if e['kind'] == 'aircraft']
    if any(sum(e['team'] == team for e in aircraft) > 1 for team in ('blue', 'red')):
        warnings.append('Aynı takımda birden fazla uçak var. BVRGym logger’ı aynı numaralı füze dosyalarını ezebilir; mühimmat sahipliği ve toplamı doğrulanamıyor.')
    if any(e.get('visual_model') == 'generic' for e in entities):
        warnings.append('Bazı modellerin geometrisi bulunmuyor; sahnede genel nesne simgesiyle gösteriliyor.')
    if any(e.get('heading_deg_known') is False for e in entities):
        warnings.append('Yönelim sütunları eksik olan nesnelerin burun yönü bilinmiyor; yön çizgisi gösterilmez.')
    return warnings


class RecordingStore:
    """Publish a complete, stable folder snapshot; keep every recorded sample."""
    def __init__(self, directory, settle_seconds=2.0, poll_seconds=.25, catalog_roots=()):
        self.directory = Path(directory).resolve()
        self.settle_seconds = settle_seconds
        self.poll_seconds = poll_seconds
        self.lock = threading.RLock()
        self.update_lock = threading.Lock()
        self.recording_name = self.directory.name
        self.active_directory = None
        roots = [Path(root).resolve() for root in catalog_roots]
        self.catalog_root = next((p for p in roots if p.is_dir()), self.directory)
        self.warnings = []
        self.samples, self.entities = [], []
        self.session = str(uuid.uuid4())
        self.phase, self.error = 'waiting', None
        self.loaded_signature = None
        self.pending_signature = None
        self.attempted_signature = None
        self.pending_since = 0
        self.stop = threading.Event()
        self.thread = None

    def refresh(self, now=None):
        with self.update_lock:
            self._refresh(now)

    def _refresh(self, now=None):
        now = time.monotonic() if now is None else now
        try:
            signature = fingerprint(self.directory)
            if signature != self.pending_signature:
                self.pending_signature, self.pending_since = signature, now
                with self.lock:
                    self.phase, self.error = 'waiting', None
            if signature == self.loaded_signature:
                with self.lock:
                    self.phase, self.error = 'ready', None
                return
            if not signature or now - self.pending_since < self.settle_seconds:
                return
            if signature == self.attempted_signature:
                return
            self.attempted_signature = signature
            samples, entities = read_recording(self.directory)
            if fingerprint(self.directory) != signature:
                self.attempted_signature = None
                return
            with self.lock:
                self.samples, self.entities = samples, entities
                self.session = str(uuid.uuid4())
                self.loaded_signature = signature
                self.active_directory = self.directory
                self.recording_name = self.directory.name
                self.phase, self.error = 'ready', None
                self.warnings = recording_warnings(entities)
        except EmptyRecording:
            with self.lock:
                self.phase, self.error = 'waiting', None
        except (OSError, ValueError, csv.Error) as exc:
            with self.lock:
                self.phase, self.error = 'error', str(exc)

    def browse(self, path=None):
        directory = Path(path).expanduser().resolve() if path else self.catalog_root
        if path and not directory.is_dir():
            raise ValueError('Dizin bulunamadı')
        while not directory.exists() and directory.parent != directory:
            directory = directory.parent
        if not directory.is_dir():
            raise ValueError('Dizin bulunamadı')
        children = []
        for child in sorted(directory.iterdir(), key=lambda p: p.name.casefold()):
            try:
                if child.is_dir():
                    children.append(dict(name=child.name, path=str(child)))
            except OSError:
                continue
        return dict(path=str(directory), parent=str(directory.parent) if directory.parent != directory else None,
                    folders=children)

    def change_directory(self, path):
        if not isinstance(path, str) or not path.strip():
            raise ValueError('Dizin yolu gerekli')
        directory = Path(path).expanduser().resolve()
        if not directory.is_dir():
            raise ValueError('Dizin bulunamadı')
        # Validate access before replacing the catalog. The open flight stays unchanged.
        list(directory.iterdir())
        with self.lock:
            self.catalog_root = directory

    def catalog(self):
        with self.lock:
            root = self.catalog_root
        directories = [root]
        try:
            directories.extend(p for p in root.iterdir() if p.is_dir())
        except OSError:
            pass
        entries = []
        for directory in sorted(directories):
            try:
                count = len([p for p in directory.glob('*.csv') if p.is_file()])
                if count:
                    key = hashlib.sha256(str(directory).encode()).hexdigest()[:24]
                    entries.append(dict(id=key, name=directory.name, path=str(directory), files=count))
            except OSError:
                continue
        return entries

    def open_catalog(self, identity):
        entry = next((entry for entry in self.catalog() if entry['id'] == identity), None)
        if entry is None:
            raise ValueError('Kayıt bulunamadı; dizini ve listeyi kontrol et')
        directory = Path(entry['path'])
        with self.update_lock, self.lock:
            # Use the same settling path as watched replacements, rather than bypassing it.
            self.directory = directory
            self.loaded_signature = self.pending_signature = self.attempted_signature = None
            self.phase, self.error = 'waiting', None
            self._refresh()

    def start(self):
        def watch():
            while not self.stop.is_set():
                self.refresh()
                self.stop.wait(self.poll_seconds)
        self.thread = threading.Thread(target=watch, name='tacview-csv-reader', daemon=True)
        self.thread.start()

    def close(self):
        self.stop.set()
        if self.thread:
            self.thread.join(timeout=5)

    def packet(self, after=0, limit=2000, session=None):
        with self.lock:
            if session is not None and session != self.session:
                after = 0
            after = min(after, len(self.samples))
            selected = self.samples[after:after + limit]
            return dict(schema_version=3, source='tacview-csv', session=self.session,
                        phase=self.phase, error=self.error, recording=str(self.active_directory or self.directory),
                        recording_name=self.recording_name, recording_mode='folder',
                        catalog_root=str(self.catalog_root), warnings=self.warnings,
                        sequence=len(self.samples), cursor=after + len(selected), samples=selected,
                        objects=len(self.entities),
                        inventory={entry['id']: entry.get('inventory', []) for entry in self.entities if entry['kind'] == 'aircraft'},
                        unassigned_inventory=self.entities[0].get('unassigned_inventory', []) if self.entities else [],
                        bounds=dict(first=self.samples[0]['sim_time_s'], last=self.samples[-1]['sim_time_s'])
                        if self.samples else None)
