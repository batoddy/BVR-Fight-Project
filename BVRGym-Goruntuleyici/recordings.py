"""Read BVRGym's native Tacview CSV exports without changing their files."""
import csv
import math
from pathlib import Path
import re
import threading
import time
import uuid


class EmptyRecording(ValueError):
    """The logger has not saved any measurements yet."""


def fingerprint(directory):
    return tuple((p.name, p.stat().st_size, p.stat().st_mtime_ns)
                 for p in sorted(directory.glob('*.csv')) if p.is_file())


def metadata(path):
    # Tacview stores model, optional callsign and optional color in the filename.
    match = re.fullmatch(r'(.+?)(?: \(([^()]*)\))?(?: \[([^\[\]]+)\])?', path.stem)
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


def read_object(path):
    info = metadata(path)
    rows = []
    previous = {}
    fields = {'Time': 'sim_time_s', 'Longitude': 'longitude_deg',
              'Latitude': 'latitude_deg', 'Altitude': 'altitude_m',
              'Roll (deg)': 'roll_deg', 'Pitch (deg)': 'pitch_deg',
              'Yaw (deg)': 'heading_deg', 'TAS': 'true_airspeed_mps', 'Mach': 'mach'}
    with path.open(encoding='utf-8-sig', newline='') as handle:
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


def read_recording(directory):
    directory = Path(directory)
    objects = [read_object(path) for path in sorted(directory.glob('*.csv')) if path.is_file()]
    objects = [rows for rows in objects if rows]
    if not objects:
        raise EmptyRecording('BVRGym Tacview CSV kaydı henüz oluşmadı')
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
    return samples, [dict(rows[0]) for rows in objects]


class RecordingStore:
    """Publish a complete, stable folder snapshot; keep every recorded sample."""
    def __init__(self, directory, settle_seconds=1.0, poll_seconds=.25):
        self.directory = Path(directory).resolve()
        self.settle_seconds = settle_seconds
        self.poll_seconds = poll_seconds
        self.lock = threading.RLock()
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
                        phase=self.phase, error=self.error, recording=str(self.directory),
                        sequence=len(self.samples), cursor=after + len(selected), samples=selected,
                        objects=len(self.entities),
                        bounds=dict(first=self.samples[0]['sim_time_s'], last=self.samples[-1]['sim_time_s'])
                        if self.samples else None)
