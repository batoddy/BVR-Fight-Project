"""Read BVRGym's native Tacview CSV exports without changing their files."""
import csv
import hashlib
import io
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
    kind = 'missile' if model.upper().startswith(('AIM', 'AAM', 'AMRAAM')) else 'aircraft'
    return dict(id=path.name, name=callsign or model, model=model, color=color,
                team=team, kind=kind, file=path.name)


def ecef(lat_deg, lon_deg, height_m):
    lat, lon = math.radians(lat_deg), math.radians(lon_deg)
    a, e2 = 6378137.0, 6.69437999014e-3
    n = a / math.sqrt(1 - e2 * math.sin(lat) ** 2)
    return ((n + height_m) * math.cos(lat) * math.cos(lon),
            (n + height_m) * math.cos(lat) * math.sin(lon),
            (n * (1 - e2) + height_m) * math.sin(lat))


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
            previous = dict(values)
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


def read_exports(files):
    if not isinstance(files, list) or not 1 <= len(files) <= 1000:
        raise ValueError('1–1000 CSV dosyası seçmelisin')
    objects, names, manifest = [], set(), []
    for entry in files:
        if not isinstance(entry, dict):
            raise ValueError('Geçersiz dosya')
        name, content = entry.get('name'), entry.get('text')
        if not isinstance(name, str) or not name.lower().endswith('.csv') or any(c in name for c in ('/', '\\', '\x00')):
            raise ValueError('Geçersiz CSV dosya adı')
        if name.casefold() in names:
            raise ValueError(f'Aynı adlı dosyalar farklı kayıtlardan gelmiş olabilir: {name}')
        if not isinstance(content, str):
            raise ValueError('CSV metni bulunamadı')
        names.add(name.casefold())
        manifest.append(metadata(Path(name)))
        objects.append(read_csv(Path(name), io.StringIO(content.lstrip('\ufeff'), newline='')))
    return assemble(objects, manifest)


def assemble(objects, manifest=()):
    objects = [rows for rows in objects if rows]
    if not objects:
        raise EmptyRecording('BVRGym Tacview CSV kaydı henüz oluşmadı')
    for rows in objects:
        estimate_motion(rows, ecef)
    samples = sorted((row for rows in objects for row in rows), key=lambda row: row['sim_time_s'])
    first = next((row for row in samples if row['kind'] == 'aircraft'), samples[0])
    lat0, lon0 = math.radians(first['latitude_deg']), math.radians(first['longitude_deg'])
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


class RecordingStore:
    """Publish a complete, stable folder snapshot; keep every recorded sample."""
    def __init__(self, directory, settle_seconds=1.0, poll_seconds=.25, catalog_roots=()):
        self.directory = Path(directory).resolve()
        self.settle_seconds = settle_seconds
        self.poll_seconds = poll_seconds
        self.lock = threading.RLock()
        self.update_lock = threading.Lock()
        self.mode = 'folder'
        self.recording_name = self.directory.name
        self.catalog_roots = [Path(root).resolve() for root in catalog_roots]
        self.initial_directory = self.directory
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
            if self.mode == 'folder':
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
                self.phase, self.error = 'ready', None
        except EmptyRecording:
            with self.lock:
                self.phase, self.error = 'waiting', None
        except (OSError, ValueError, csv.Error) as exc:
            with self.lock:
                self.phase, self.error = 'error', str(exc)

    def catalog(self):
        directories = {self.initial_directory, self.directory}
        for root in self.catalog_roots:
            try:
                if root.is_dir():
                    directories.add(root)
                    directories.update(path for path in root.iterdir() if path.is_dir())
            except OSError:
                continue
        entries = []
        for directory in sorted(directories):
            try:
                count = len(list(directory.glob('*.csv')))
                if count:
                    key = hashlib.sha256(str(directory).encode()).hexdigest()[:24]
                    entries.append(dict(id=key, name=directory.name, path=str(directory), files=count))
            except OSError:
                continue
        return entries

    def import_files(self, files, title='Seçilen CSV dosyaları'):
        if not isinstance(title, str) or not 1 <= len(title.strip()) <= 160:
            raise ValueError('Geçersiz kayıt adı')
        samples, entities = read_exports(files)
        with self.update_lock, self.lock:
            self.mode = 'files'
            self.recording_name = title.strip()
            self.samples, self.entities = samples, entities
            self.session = str(uuid.uuid4())
            self.phase, self.error = 'ready', None

    def open_catalog(self, identity):
        entry = next((entry for entry in self.catalog() if entry['id'] == identity), None)
        if entry is None:
            raise ValueError('Kayıt bulunamadı; listeyi yenile')
        directory = Path(entry['path'])
        with self.update_lock:
            signature = fingerprint(directory)
            samples, entities = read_recording(directory)
            if signature != fingerprint(directory):
                raise ValueError('Kayıt dosyaları yazılıyor; biraz sonra yeniden aç')
            with self.lock:
                self.directory, self.mode = directory, 'folder'
                self.recording_name = directory.name
                self.samples, self.entities = samples, entities
                self.session = str(uuid.uuid4())
                self.loaded_signature = self.pending_signature = self.attempted_signature = signature
                self.phase, self.error = 'ready', None

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
            return dict(schema_version=2, source='tacview-csv', session=self.session,
                        phase=self.phase, error=self.error, recording=str(self.directory) if self.mode == 'folder' else '',
                        recording_name=self.recording_name, recording_mode=self.mode,
                        sequence=len(self.samples), cursor=after + len(selected), samples=selected,
                        objects=len(self.entities),
                        inventory={entry['id']: entry.get('inventory', []) for entry in self.entities if entry['kind'] == 'aircraft'},
                        unassigned_inventory=self.entities[0].get('unassigned_inventory', []) if self.entities else [],
                        bounds=dict(first=self.samples[0]['sim_time_s'], last=self.samples[-1]['sim_time_s'])
                        if self.samples else None)
