"""C++ hesapları: kaydedici yazar, görüntüleyici aynı zamanlı uçak ölçümüne bağlar."""
import csv
import importlib.util
from pathlib import Path
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from recordings import RecordingStore, read_recording

spec = importlib.util.spec_from_file_location('sim_recorder', ROOT.parent / 'src' / 'sim' / 'recorder.py')
sim_recorder = importlib.util.module_from_spec(spec)
spec.loader.exec_module(sim_recorder)

OWN_HEADER = ['time_s', 'lon_deg', 'lat_deg', 'alt_m', 'roll_deg', 'pitch_deg', 'yaw_deg']


def calc(agent, target, ata):
    return dict(id=agent, target_id=target, range_2d_m=9000.0, range_3d_m=9100.0, bearing_deg=10.0,
                ata_deg=ata, aspect_deg=170.0, is_self_in_enemy_wez=True, mar_m=0.0, dor_m=0.0,
                radar_gimbal_deg=60.0, radar_range_m=80000.0, wez_min_range_m=2000.0, wez_max_range_m=31000.0)


class CalculationChecks(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()

    def tearDown(self):
        self.temp.cleanup()

    def flight(self, recorder, times):
        # Uçak CSV'lerini kaydedicinin yazacağı adla ve aynı time_s değerleriyle yaz.
        for key, name, team, lon in (('blue', 'KE', 'Blue', 18.0), ('red', 'Hostile', 'Red', 18.2)):
            with (recorder.directory / f'F-16 ({name}--{key}) [{team}].csv').open('w', newline='', encoding='utf-8') as handle:
                writer = csv.writer(handle)
                writer.writerow(OWN_HEADER)
                for index, time_s in enumerate(times):
                    writer.writerow([time_s, lon + index * .001, 59, 7000, 0, 0, 90])

    def test_recorder_stamps_last_measurement_time_and_viewer_matches_exactly(self):
        recorder = sim_recorder.Recorder(self.temp.name)
        times = [0.0, 0.08333333333333333, 0.16666666666666666]
        self.flight(recorder, times)
        # Python her step: record() -> state gönder -> C++ hesabı -> record_calculations().
        for index, time_s in enumerate(times[:-1]):
            recorder.last_time = {'blue': time_s, 'red': time_s}
            recorder.record_calculations({'aircraft': [calc('blue', 'red', index), calc('red', 'blue', -index)]})
        recorder.close()
        with (recorder.directory / 'calculations.csv').open(encoding='utf-8') as handle:
            rows = list(csv.DictReader(handle))
        self.assertEqual([row['time_s'] for row in rows if row['id'] == 'blue'], [str(t) for t in times[:-1]])
        self.assertEqual(rows[0]['is_self_in_enemy_wez'], '1')

        samples, entities = read_recording(recorder.directory)
        self.assertEqual(len(entities), 2)  # calculations.csv nesne sayılmaz
        blue = [row for row in samples if row.get('agent_key') == 'blue']
        self.assertEqual([row['calc']['ata_deg'] for row in blue[:2]], [0.0, 1.0])
        self.assertNotIn('calc', blue[2])  # son ölçümün hesabı yok; uydurulmaz
        self.assertEqual(blue[0]['calc']['target_entity_id'], 'F-16 (Hostile--red) [Red].csv')
        self.assertIs(blue[0]['calc']['is_self_in_enemy_wez'], True)

        store = RecordingStore(self.temp.name, catalog_roots=[self.temp.name])
        self.assertEqual(store.catalog()[0]['files'], 2)

    def test_recorder_rejects_unknown_aircraft_and_changed_fields(self):
        recorder = sim_recorder.Recorder(self.temp.name)
        try:
            recorder.record_calculations(None)  # eski otopilot: dosya açılmaz
            self.assertFalse((recorder.directory / 'calculations.csv').exists())
            recorder.last_time = {'blue': 0.0}
            with self.assertRaises(ValueError):
                recorder.record_calculations({'aircraft': [calc('green', 'blue', 0)]})
            recorder.record_calculations({'aircraft': [calc('blue', 'red', 0)]})
            changed = calc('blue', 'red', 0)
            changed['extra_m'] = 1.0
            with self.assertRaises(ValueError):
                recorder.record_calculations({'aircraft': [changed]})
        finally:
            recorder.close()


if __name__ == '__main__':
    unittest.main()
