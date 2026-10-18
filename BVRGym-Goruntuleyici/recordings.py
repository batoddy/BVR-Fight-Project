"""Read Tacview-named CSV recordings without changing their files.

Two producers are supported: our own recorder (Workspace/src/recorder.py), which
carries the agent key in the filename and ships a manifest.json, and BVRGym's
built-in TacviewLogger, which does neither.
"""
import csv
import hashlib
import json
import math
from pathlib import Path
import re
import threading
import time
import uuid
from telemetry import estimate_motion


class EmptyRecording(ValueError):
    """The logger has not saved any measurements yet."""


SIDECARS = ('events.jsonl', 'manifest.json')
EVENT_LIMIT = 5000


def fingerprint(directory):
    paths = list(directory.glob('*.csv')) + [directory / name for name in SIDECARS]
    return tuple((p.name, p.stat().st_size, p.stat().st_mtime_ns)
                 for p in sorted(paths) if p.is_file())


def recording_files(directory):
    """CSV files and sidecars that make up one folder recording."""
    paths = [p for p in directory.glob('*.csv') if p.is_file()]
    return paths + [directory / name for name in SIDECARS if (directory / name).is_file()]


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
    # Our recorder writes "KE--blue" for aircraft and "KE--blue--0" for munitions,
    # so the launcher is stated rather than guessed. Built-in logs have one part.
    parts = (callsign or model).split('--')
    agent_key = parts[1] if kind == 'aircraft' and len(parts) > 1 else None
    carrier_key = parts[1] if kind == 'missile' and len(parts) > 2 else None
    ammo_id = parts[2] if kind == 'missile' and len(parts) > 2 else None
    name = f'{parts[0]}-{ammo_id}' if ammo_id is not None else parts[0]
    return dict(id=path.name, name=name, model=model, color=color,
                team=team, kind=kind, visual_model=visual_model, file=path.name,
                agent_key=agent_key, carrier_key=carrier_key, ammo_id=ammo_id)


def ecef(lat_deg, lon_deg, height_m):
    """WGS84 geodetic latitude and ellipsoid height -> ECEF."""
    lat, lon = math.radians(lat_deg), math.radians(lon_deg)
    a, e2 = 6378137.0, 6.69437999014e-3
    n = a / math.sqrt(1 - e2 * math.sin(lat) ** 2)
    return ((n + height_m) * math.cos(lat) * math.cos(lon),
            (n + height_m) * math.cos(lat) * math.sin(lon),
            (n * (1 - e2) + height_m) * math.sin(lat))


def geodetic_latitude(lat_gc_deg, height_m):
    """BVRGym native geocentric latitude and JSBSim radial ASL -> WGS84 geodetic.
    Only recordings that still name the column `Latitude` need this; our own
    recorder writes position/lat-geod-deg straight into `lat_deg`."""
    lat = math.radians(lat_gc_deg)
    a, b = 6378137.0, 6356752.314245
    sea_radius = 1 / math.sqrt((math.cos(lat) / a) ** 2 + (math.sin(lat) / b) ** 2)
    radius = sea_radius + height_m
    x, z = radius * math.cos(lat), radius * math.sin(lat)
    e2 = 6.69437999014e-3
    phi = math.atan2(z, (1 - e2) * x)
    for _ in range(8):
        n = a / math.sqrt(1 - e2 * math.sin(phi) ** 2)
        phi = math.atan2(z + e2 * n * math.sin(phi), x)
    return math.degrees(phi)


# Recorded measurements that only need a unit change to display, never a value
# derived from other columns. The plain TAS/Mach columns above take precedence.
SOURCE_FIELDS = {'tas_fps': ('true_airspeed_mps', 0.3048),
                 'velocities/vt-fps': ('true_airspeed_mps', 0.3048),
                 'velocities/mach': ('mach', 1.0)}

# Our recorder (schema 2) names its own columns and states WGS84 geodetic
# latitude; BVRGym's built-in TacviewLogger keeps Tacview names and geocentric
# latitude. The column name tells which, so the reference is never guessed.
OWN_FIELDS = {'time_s': 'sim_time_s', 'lon_deg': 'longitude_deg',
              'lat_deg': 'latitude_deg', 'alt_m': 'altitude_m',
              'roll_deg': 'roll_deg', 'pitch_deg': 'pitch_deg',
              'yaw_deg': 'heading_deg', 'mach': 'mach'}
TACVIEW_FIELDS = {'Time': 'sim_time_s', 'Longitude': 'longitude_deg',
                  'Latitude': 'latitude_gc_deg', 'Altitude': 'altitude_m',
                  'Roll (deg)': 'roll_deg', 'Pitch (deg)': 'pitch_deg',
                  'Yaw (deg)': 'heading_deg', 'TAS': 'true_airspeed_mps',
                  'Mach': 'mach'}
OWN_REQUIRED = ('time_s', 'lon_deg', 'lat_deg', 'alt_m')
TACVIEW_REQUIRED = ('Time', 'Longitude', 'Latitude', 'Altitude')


def read_csv(path, handle):
    info = metadata(path)
    rows = []
    previous = {}
    reader = csv.DictReader(handle)
    # Native pandas exports unused ammunition as an empty file / blank line.
    if not reader.fieldnames:
        return []
    reader.fieldnames = [key.strip() for key in reader.fieldnames]
    if len(set(reader.fieldnames)) != len(reader.fieldnames):
        raise ValueError(f'{path.name}: yinelenen sütun adı')
    names = set(reader.fieldnames)
    own = set(OWN_REQUIRED) <= names
    if own:
        fields = dict(OWN_FIELDS)
    elif set(TACVIEW_REQUIRED) <= names:
        fields = dict(TACVIEW_FIELDS)
    else:
        raise ValueError(f'{path.name}: {", ".join(OWN_REQUIRED)} ya da '
                         f'{", ".join(TACVIEW_REQUIRED)} sütunları gerekli')
    time_column = 'time_s' if own else 'Time'
    # `Latitude` is geocentric; `lat_deg` already is the geodetic value.
    geocentric = not own
    if not own:
        for key in ('Roll', 'Pitch', 'Yaw'):
            if key in names and key + ' (deg)' not in names:
                fields[key] = fields.pop(key + ' (deg)')
    for row in reader:
        if not any(value for value in row.values()):
            continue
        try:
            if None in row:
                raise ValueError('sütun sayısı uyuşmuyor')
            values = dict(previous)
            measured = set()
            for column, key in fields.items():
                raw = row.get(column)
                if raw is not None and raw.strip():
                    value = float(raw)
                    if not math.isfinite(value):
                        raise ValueError('sonlu olmayan sayı')
                    values[key] = value
                    measured.add(key)
            for column, (key, scale) in SOURCE_FIELDS.items():
                raw = row.get(column)
                if key in measured or raw is None or not raw.strip():
                    continue
                value = float(raw) * scale
                if not math.isfinite(value):
                    raise ValueError('sonlu olmayan sayı')
                values[key] = value
            if geocentric and 'latitude_gc_deg' in values and 'altitude_m' in values:
                # Derived from the raw column every row, never from the previous
                # result, so carrying a value forward cannot convert it twice.
                if not -90 <= values['latitude_gc_deg'] <= 90:
                    raise ValueError('koordinat aralık dışında')
                values['latitude_deg'] = geodetic_latitude(values['latitude_gc_deg'],
                                                           values['altitude_m'])
            if not (row.get(time_column) or '').strip():
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


def read_manifest(directory):
    """Report what the recording states about itself, so a column is never decoded
    in the wrong reference. Missing manifest means the built-in logger wrote it."""
    path = Path(directory) / 'manifest.json'
    if not path.is_file():
        return None, []
    try:
        manifest = json.loads(path.read_text(encoding='utf-8'))
        if not isinstance(manifest, dict):
            raise ValueError('manifest bir nesne değil')
        coordinates = manifest.get('coordinates') or {}
    except (ValueError, OSError):
        return None, ['manifest.json okunamadı; koordinat referansı doğrulanamıyor.']
    warnings = []
    # Both references are decoded: `lat_deg` is read as geodetic, the older
    # `Latitude` column as geocentric. A third reference would not be.
    if coordinates.get('latitude_reference') not in (None, 'geocentric', 'WGS84 geodetic'):
        warnings.append(f"Kayıt enlemi '{coordinates['latitude_reference']}' olarak bildiriyor; "
                        'bu görüntüleyici jeodezik ve jeosantrik enlem çözer. Konumlar kayabilir.')
    if coordinates.get('altitude_reference') not in (None, 'JSBSim radial MSL',
                                                     'WGS84 ellipsoid height'):
        warnings.append(f"Kayıt irtifayı '{coordinates['altitude_reference']}' olarak bildiriyor; "
                        'bu görüntüleyici WGS84 elipsoid yüksekliği çözer.')
    return manifest, warnings


def read_events(directory):
    """Read the recorded event log. Events are facts the recording states; the
    viewer never infers one from measurements."""
    path = Path(directory) / 'events.jsonl'
    if not path.is_file():
        return [], []
    events, warnings = [], []
    for number, line in enumerate(path.read_text(encoding='utf-8').splitlines(), 1):
        if not line.strip():
            continue
        try:
            event = json.loads(line)
            if not isinstance(event, dict):
                raise ValueError('satır bir nesne değil')
            time_s, kind = event['sim_time_s'], event['type']
            if not isinstance(time_s, (int, float)) or not math.isfinite(time_s):
                raise ValueError('sim_time_s sayı değil')
            if not isinstance(kind, str) or not kind:
                raise ValueError('type eksik')
        except (KeyError, TypeError, ValueError) as exc:
            raise ValueError(f'events.jsonl, satır {number}: {exc}') from exc
        events.append(event)
    events.sort(key=lambda event: (event['sim_time_s'], event.get('seq', 0)))
    if len(events) > EVENT_LIMIT:
        warnings.append(f'Kayıtta {len(events)} olay var; ilk {EVENT_LIMIT} tanesi gösteriliyor.')
        events = events[:EVENT_LIMIT]
    return events, warnings


def attach_outcomes(entities, events):
    """Mark each munition with the recorded outcome. Without an event log the
    munition keeps its measurement-only state; an outcome is never guessed."""
    outcomes = {}
    for event in events:
        if event['type'] in ('missile_hit', 'missile_lost') and event.get('ammo') is not None:
            outcomes[(event.get('agent'), str(event['ammo']))] = event
    for entry in entities:
        for munition in entry.get('inventory', []) + entry.get('unassigned_inventory', []):
            event = outcomes.get((munition.get('carrier_key'), munition.get('ammo_id')))
            munition['outcome'] = None if event is None else event['type'].split('_')[1]
            munition['outcome_time_s'] = None if event is None else event['sim_time_s']


def session_info(manifest):
    """What the recording says about itself, in the shape the session panel shows.
    Nothing is filled in when the recording stays silent about a field."""
    if not isinstance(manifest, dict):
        return None
    simulation = manifest.get('simulation') or {}
    coordinates = manifest.get('coordinates') or {}
    scenario = manifest.get('scenario') or {}
    world = scenario.get('world') if isinstance(scenario, dict) else None
    agents = scenario.get('agents') if isinstance(scenario, dict) else None
    return dict(
        schema_version=manifest.get('schema_version'),
        created_at=manifest.get('created_at'),
        snapshots=manifest.get('snapshots'),
        events=manifest.get('events'),
        conversions=manifest.get('conversions'),
        latitude_reference=coordinates.get('latitude_reference'),
        altitude_reference=coordinates.get('altitude_reference'),
        coordinate_note=coordinates.get('note'),
        physics_dt_s=simulation.get('physics_dt_s'),
        aircraft_sim_time_step=simulation.get('aircraft_sim_time_step'),
        aircraft_control_time_step=simulation.get('aircraft_control_time_step'),
        duration_s=(world or {}).get('duration_s'),
        sleep_s=(world or {}).get('sleep_s'),
        agent_count=len(agents) if isinstance(agents, dict) else None,
        columns=manifest.get('columns') if isinstance(manifest.get('columns'), dict) else None,
    )


def read_recording(directory):
    directory = Path(directory)
    paths = [path for path in sorted(directory.glob('*.csv')) if path.is_file()]
    samples, entities = assemble([read_object(path) for path in paths],
                                 [metadata(path) for path in paths])
    manifest, warnings = read_manifest(directory)
    events, event_warnings = read_events(directory)
    attach_outcomes(entities, events)
    if entities:
        entities[0]['manifest'] = manifest
        entities[0]['events'] = events
        entities[0]['source_warnings'] = warnings + event_warnings
    return samples, entities


def assemble(objects, manifest=()):
    objects = [rows for rows in objects if rows]
    if not objects:
        raise EmptyRecording('BVRGym Tacview CSV kaydı henüz oluşmadı')
    for rows in objects:
        estimate_motion(rows, ecef)
    samples = sorted((row for rows in objects for row in rows), key=lambda row: row['sim_time_s'])
    first = next((row for row in samples if row['kind'] == 'aircraft'), samples[0])
    lat0 = math.radians(first['latitude_deg'])  # read_csv normalised it to geodetic
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
        owner, source = None, None
        if info.get('carrier_key'):
            stated = [entry for entry in aircraft if entry.get('agent_key') == info['carrier_key']]
            if len(stated) == 1:
                owner, source = stated[0]['id'], 'filename'
        if owner is None:
            # Built-in filenames encode team, but no launcher ID. Never guess among teammates.
            candidates = [entry for entry in aircraft if info['color'] and entry['color'].casefold() == info['color'].casefold()]
            if len(candidates) == 1:
                owner, source = candidates[0]['id'], 'team'
        track = tracks.get(info['id'])
        munition = dict(info, owner_id=owner, ownership_source=source,
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
    warnings = list(entities[0].get('source_warnings', []) if entities else [])
    if entities and entities[0].get('unassigned_inventory'):
        warnings.append('Bazı mühimmat dosyalarının taşıyıcısı kayıtta belirtilmemiş. BVRGym logger’ı aynı numaralı füze dosyalarını ezebilir; sahiplik ve toplam doğrulanamıyor.')
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
                files = recording_files(directory)
                count = sum(1 for p in files if p.suffix.lower() == '.csv')
                if count:
                    key = hashlib.sha256(str(directory).encode()).hexdigest()[:24]
                    modified = max(p.stat().st_mtime for p in files)
                    entries.append(dict(id=key, name=directory.name, path=str(directory), files=count,
                                        modified=modified))
            except OSError:
                continue
        # En yeni kayıt en üstte; aynı anda yazılanlar ada göre sabit sırada kalır.
        entries.sort(key=lambda entry: (-entry['modified'], entry['name']))
        return entries

    def delete_catalog(self, identity):
        entry = next((entry for entry in self.catalog() if entry['id'] == identity), None)
        if entry is None:
            raise ValueError('Kayıt bulunamadı; dizini ve listeyi kontrol et')
        directory = Path(entry['path'])
        # Yalnızca kayıt dosyaları silinir; klasör başka dosya içermiyorsa o da kaldırılır.
        for path in recording_files(directory):
            path.unlink()
        with self.lock:
            root = self.catalog_root
        if directory != root and not any(directory.iterdir()):
            directory.rmdir()

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
                        events=self.entities[0].get('events', []) if self.entities else [],
                        manifest=session_info(self.entities[0].get('manifest')) if self.entities else None,
                        bounds=dict(first=self.samples[0]['sim_time_s'], last=self.samples[-1]['sim_time_s'])
                        if self.samples else None)
