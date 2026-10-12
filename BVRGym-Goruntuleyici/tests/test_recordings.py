import csv
import hashlib
import json
from pathlib import Path
import sys
import tempfile
import threading
import unittest
from urllib.error import HTTPError
from urllib.request import Request, urlopen

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from recordings import RecordingStore, read_recording
from server import make_server

HEADER = ['Time', 'Longitude', 'Latitude', 'Altitude', 'Roll (deg)', 'Pitch (deg)', 'Yaw (deg)']
# Our own recorder (schema 2): no spaces, unit in the name, geodetic latitude.
OWN_HEADER = ['time_s', 'lon_deg', 'lat_deg', 'alt_m', 'roll_deg', 'pitch_deg', 'yaw_deg']


class RecordingChecks(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.directory = Path(self.temp.name)

    def tearDown(self):
        self.temp.cleanup()

    def write(self, name='F-16 (Blue) [Blue].csv', rows=None, header=HEADER):
        path = self.directory / name
        with path.open('w', encoding='utf-8', newline='') as handle:
            writer = csv.writer(handle)
            writer.writerow(header)
            writer.writerows(rows or [[0, 18, 59, 7000, 0, 0, 359], [20, 18.1, 59, 7100, 10, 2, 1]])
        return path

    def ready(self):
        store = RecordingStore(self.directory, settle_seconds=1)
        store.refresh(0)
        store.refresh(1.1)
        self.assertEqual(store.phase, 'ready', store.error)
        return store

    def test_native_aircraft_and_missile_share_clock(self):
        self.write()
        self.write('F-16 (Bogey) [Red].csv')
        self.write('AIM-120 AMRAAM (AIM0) [Blue].csv', [[10, 18.02, 59, 7000, 0, 0, 0], [15, 18.04, 59, 7500, 0, 1, 5]])
        samples, objects = read_recording(self.directory)
        self.assertEqual(len(objects), 3)
        missile = next(row for row in samples if row['kind'] == 'missile')
        self.assertEqual((missile['record_start_s'], missile['record_end_s']), (10, 15))
        self.assertIsNone(missile['mach'])
        self.assertIsNone(missile['true_airspeed_mps'])
        self.assertEqual(missile['team'], 'blue')
        self.assertEqual([row['sim_time_s'] for row in samples], [0, 0, 10, 15, 20, 20])

    def test_own_column_names_are_read_as_geodetic(self):
        """`lat_deg` is already WGS84, so it is used as written; the older
        `Latitude` column is geocentric and gets converted. Both must place the
        same JSBSim state at the same point."""
        self.write(rows=[[0, 18, 59.418960, 3000, 0, 0, 180]], header=OWN_HEADER)
        own = read_recording(self.directory)[0][0]
        self.assertAlmostEqual(own['latitude_deg'], 59.418960, places=9)
        self.temp.cleanup()
        self.temp = tempfile.TemporaryDirectory()
        self.directory = Path(self.temp.name)
        self.write(rows=[[0, 18, 59.250206, 3000, 0, 0, 180]])
        legacy = read_recording(self.directory)[0][0]
        self.assertAlmostEqual(legacy['latitude_deg'], own['latitude_deg'], places=5)

    def test_own_recording_missing_a_required_column_is_reported(self):
        self.write(rows=[[0, 18, 3000]], header=['time_s', 'lon_deg', 'alt_m'])
        with self.assertRaisesRegex(ValueError, 'time_s'):
            read_recording(self.directory)

    def test_reading_does_not_modify_csv_or_import_simulator(self):
        path = self.write()
        before = hashlib.sha256(path.read_bytes()).hexdigest()
        read_recording(self.directory)
        self.assertEqual(hashlib.sha256(path.read_bytes()).hexdigest(), before)
        self.assertNotIn('jsb_gym', sys.modules)

    def test_unused_ammunition_files_are_skipped(self):
        self.write()
        (self.directory / 'AIM-120 AMRAAM (AIM1) [Red].csv').write_text('\n')
        self.assertEqual(len(read_recording(self.directory)[1]), 1)

    def test_columns_can_be_reordered_and_optional_values_are_unknown(self):
        self.write(header=['Altitude', 'Time', 'Latitude', 'Longitude'], rows=[[5000, 0, 59, 18]])
        row = read_recording(self.directory)[0][0]
        self.assertEqual(row['altitude_m'], 5000)
        self.assertEqual(row['heading_deg'], 0)
        self.assertIsNone(row['mach'])

    def test_optional_tas_and_mach_are_used_when_present(self):
        self.write(header=HEADER + ['TAS', 'Mach'], rows=[[0, 18, 59, 5000, 0, 0, 90, 250, .8]])
        row = read_recording(self.directory)[0][0]
        self.assertEqual((row['true_airspeed_mps'], row['mach']), (250, .8))

    def test_sparse_tacview_values_keep_previous_measurement(self):
        self.write(rows=[[0, 18, 59, 5000, 0, 0, 90], [1, '', '', 5100, '', '', '']])
        row = read_recording(self.directory)[0][-1]
        self.assertEqual((row['longitude_deg'], row['heading_deg']), (18, 90))

    def test_duplicate_time_keeps_last_measurement(self):
        self.write(rows=[[0, 18, 59, 5000, 0, 0, 90], [0, 18, 59, 5100, 0, 0, 91]])
        rows = read_recording(self.directory)[0]
        self.assertEqual(len(rows), 1)
        self.assertEqual(rows[0]['altitude_m'], 5100)

    def test_invalid_measurement_reports_file_and_line(self):
        for bad in (float('nan'), float('inf'), 'broken'):
            self.write(rows=[[0, 18, 59, bad, 0, 0, 0]])
            with self.assertRaisesRegex(ValueError, 'satır 2'):
                read_recording(self.directory)

    def test_backward_time_and_missing_columns_are_rejected(self):
        self.write(rows=[[2, 18, 59, 5000, 0, 0, 0], [1, 18, 59, 5000, 0, 0, 0]])
        with self.assertRaisesRegex(ValueError, 'geriye'):
            read_recording(self.directory)
        self.write(header=['Time'], rows=[[0]])
        with self.assertRaisesRegex(ValueError, 'sütunları gerekli'):
            read_recording(self.directory)

    def test_viewer_can_start_before_logger_saves(self):
        store = RecordingStore(self.directory, settle_seconds=1)
        store.refresh(0)
        self.assertEqual(store.packet()['phase'], 'waiting')
        self.write()
        store.refresh(.5)
        self.assertEqual(store.packet()['sequence'], 0)
        store.refresh(1.6)
        self.assertEqual(store.packet()['sequence'], 2)

    def test_folder_changes_are_published_only_when_stable(self):
        self.write()
        store = self.ready()
        session = store.session
        self.write(rows=[[0, 18, 59, 6000, 0, 0, 0]])
        store.refresh(2)
        self.assertEqual(store.session, session)
        store.refresh(2.5)
        self.assertEqual(store.session, session)
        store.refresh(3.1)
        self.assertNotEqual(store.session, session)
        self.assertEqual(store.packet(after=100, session=session)['cursor'], 1)
        self.assertEqual(store.packet()['samples'][0]['altitude_m'], 6000)

    def test_invalid_replacement_preserves_valid_recording_and_recovers(self):
        self.write()
        store = self.ready()
        session = store.session
        self.write(rows=[[0, 18, 59, 'bad', 0, 0, 0]])
        store.refresh(2)
        store.refresh(3.1)
        self.assertEqual(store.phase, 'error')
        self.assertEqual(store.session, session)
        self.assertEqual(len(store.samples), 2)
        self.write(rows=[[0, 18, 59, 6500, 0, 0, 0]])
        store.refresh(4)
        store.refresh(5.1)
        self.assertEqual(store.phase, 'ready')
        self.assertNotEqual(store.session, session)

    def test_long_record_keeps_all_samples_and_paginates(self):
        self.write(rows=[[index, 18, 59, 5000, 0, 0, 0] for index in range(25001)])
        store = self.ready()
        self.assertEqual(len(store.samples), 25001)
        cursor, count = 0, 0
        while cursor < 25001:
            packet = store.packet(cursor, session=store.session)
            self.assertLessEqual(len(packet['samples']), 2000)
            count += len(packet['samples'])
            cursor = packet['cursor']
        self.assertEqual(count, 25001)
        self.assertEqual(store.packet()['bounds'], {'first': 0, 'last': 25000})

    def test_missing_directory_is_waiting_not_failure(self):
        store = RecordingStore(self.directory / 'not-created')
        store.refresh(0)
        self.assertEqual(store.phase, 'waiting')

    def test_directory_switch_changes_library_but_preserves_open_flight(self):
        original = self.write()
        store = self.ready()
        session = store.session
        library = self.directory / 'recordings'
        flight = library / 'flight1'
        flight.mkdir(parents=True)
        (flight / original.name).write_bytes(original.read_bytes())
        store.change_directory(str(library))
        self.assertEqual(store.session, session)
        self.assertEqual(len(store.catalog()), 1)
        with self.assertRaises(ValueError):
            store.change_directory(str(library / 'missing'))
        store.open_catalog(store.catalog()[0]['id'])
        self.assertEqual(store.session, session)
        self.assertEqual(store.phase, 'waiting')
        store.refresh(store.pending_since + 1.1)
        self.assertNotEqual(store.session, session)
        self.assertEqual(store.active_directory, flight.resolve())
        self.assertEqual(original.read_bytes(), (flight / original.name).read_bytes())

    def test_http_directory_and_catalog_never_offer_flight_controls(self):
        self.write()
        store = self.ready()
        server = make_server(store, 0)
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()
        base = f'http://127.0.0.1:{server.server_port}'
        try:
            with urlopen(base + '/api/recordings') as response:
                entries = json.load(response)
            self.assertEqual(len(entries), 1)
            data = json.dumps(dict(id=entries[0]['id'])).encode()
            request = Request(base + '/api/open', data=data, headers={'Content-Type': 'application/json'})
            with urlopen(request) as response:
                self.assertTrue(json.load(response)['ok'])
            before = store.session
            request.add_header('Origin', 'http://foreign.example')
            with self.assertRaises(HTTPError) as caught:
                urlopen(request)
            self.assertEqual(caught.exception.code, 403)
            self.assertEqual(store.session, before)
            bad = Request(base + '/api/directory', data=b'{"path":""}', headers={'Content-Type': 'application/json'})
            with self.assertRaises(HTTPError) as caught:
                urlopen(bad)
            self.assertEqual(caught.exception.code, 400)
            self.assertIn('error', json.load(caught.exception))
            self.assertEqual(store.session, before)
        finally:
            server.shutdown()
            server.server_close()
            thread.join()

    def test_http_validates_cursor_and_session_and_rejects_flight_controls(self):
        self.write()
        store = self.ready()
        server = make_server(store, 0)
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()
        base = f'http://127.0.0.1:{server.server_port}'
        try:
            with urlopen(base + '/api/state?after=999&session=old') as response:
                packet = json.load(response)
                self.assertEqual(packet['source'], 'tacview-csv')
                self.assertEqual(len(packet['samples']), 2)
            for path, expected in (('/api/state?after=-1', 400), ('/api/state?after=x', 400), ('/../../viewer.py', 404)):
                with self.assertRaises(HTTPError) as caught:
                    urlopen(base + path)
                self.assertEqual(caught.exception.code, expected)
            with self.assertRaises(HTTPError) as caught:
                urlopen(Request(base + '/api/state', data=b'{}', method='POST'))
            self.assertEqual(caught.exception.code, 405)
        finally:
            server.shutdown()
            server.server_close()
            thread.join()


if __name__ == '__main__':
    unittest.main(verbosity=2)
