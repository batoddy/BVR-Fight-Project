"""ft/NM/kt kayıtlarının yazımı, eski kayıtlarla uyumu ve görüntüleyici sınırı."""
import csv
import importlib.util
import json
from pathlib import Path
import sys
import tempfile
from types import SimpleNamespace
import unittest

VIEWER = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(VIEWER))
from recordings import read_recording

spec = importlib.util.spec_from_file_location('imperial_recorder', VIEWER.parent / 'src/sim/recorder.py')
recorder_module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(recorder_module)


class FakeFdm(dict):
    def get_property_catalog(self):
        return [name + ' (R)' for name in self]

    def get_delta_t(self):
        return 1 / 120


def model():
    fdm = FakeFdm({name: 0.0 for _, name, _, _ in
                   recorder_module.POSE_COLUMNS + recorder_module.EXTRA_COLUMNS + recorder_module.MISSILE_COLUMNS})
    fdm.update({
        'simulation/sim-time-sec': 1.0,
        'position/lat-geod-deg': 59.0,
        'position/long-gc-deg': 18.0,
        'position/h-sl-ft': 10000.0,
        'velocities/vtrue-kts': 500.0,
        'velocities/v-north-fps': 1000.0,
        'velocities/v-east-fps': -50.0,
        'velocities/v-down-fps': 10.0,
    })
    return SimpleNamespace(fdm=fdm, aircraft_simulation_config=SimpleNamespace(Sim_time_step=1, Control_time_step=10))


class ImperialUnitsChecks(unittest.TestCase):
    def test_recorder_writes_feet_knots_and_nm_calculations(self):
        aircraft, missile = model(), model()
        missile.active, missile.alive = True, True
        missile.target_hit = missile.target_lost = False
        missile.target = None
        missile.missile_simulation_config = SimpleNamespace(Sim_time_step=1, Control_time_step=10)
        agent = SimpleNamespace(conf=SimpleNamespace(agent_name='Test', team='Blue'),
                                simObj=aircraft, ammo={'0': missile}, target=None, healthPoints=1.0)
        with tempfile.TemporaryDirectory() as directory:
            with recorder_module.Recorder(directory) as recorder:
                recorder.record({'blue': agent}, {'blue': [90, 3048, .7]})
                recorder.record_calculations({'aircraft': [dict(
                    id='blue', target_id='red', range_3d_nm=12.0, range_2d_nm=11.0,
                    los_down_nm=-1.0, rate_of_closure_kt=600.0, enemy_in_my_wez=True,
                    radar_range_nm=40.0, wez_min_range_nm=1.0, wez_max_range_nm=25.0)]})
            aircraft_path = next(recorder.directory.glob('F-16*.csv'))
            with aircraft_path.open(encoding='utf-8', newline='') as stream:
                row, = csv.DictReader(stream)
            self.assertEqual(float(row['alt_ft']), 10000)
            self.assertEqual(float(row['cmd_alt_ft']), 10000)
            self.assertEqual(float(row['tas_kt']), 500)
            self.assertAlmostEqual(float(row['vel_north_kt']), 1000 * .3048 * 3600 / 1852)
            self.assertAlmostEqual(float(row['vel_east_kt']), -50 * .3048 * 3600 / 1852)
            self.assertNotIn('alt_m', row)
            self.assertNotIn('tas_fps', row)

            with (recorder.directory / 'calculations.csv').open(encoding='utf-8', newline='') as stream:
                calc, = csv.DictReader(stream)
            self.assertEqual(float(calc['time_s']), float(row['time_s']))
            self.assertEqual(float(calc['range_3d_nm']), 12)
            self.assertNotIn('range_3d_m', calc)
            manifest = json.loads((recorder.directory / 'manifest.json').read_text(encoding='utf-8'))
            self.assertEqual(manifest['schema_version'], 3)
            self.assertEqual(manifest['columns']['alt_ft']['unit'], 'ft')
            self.assertEqual(manifest['columns']['vel_north_kt']['unit'], 'kt')
            self.assertAlmostEqual(manifest['columns']['vel_north_kt']['scale'], .3048 * 3600 / 1852)
            self.assertEqual(manifest['calculations']['units']['distance'], 'NM')

            samples, entities = read_recording(recorder.directory)
            self.assertEqual(len(entities), 2)
            own = next(sample for sample in samples if sample['kind'] == 'aircraft')
            self.assertAlmostEqual(own['altitude_m'], 3048)
            self.assertAlmostEqual(own['true_airspeed_mps'], 500 * 1852 / 3600)
            self.assertAlmostEqual(own['local_velocity_north_mps'], 304.8)
            self.assertAlmostEqual(own['local_velocity_east_mps'], -15.24)
            self.assertAlmostEqual(own['local_velocity_down_mps'], 3.048)
            self.assertEqual(own['calc']['range_3d_nm'], 12)
            self.assertEqual(own['calc']['range_3d_m'], 12 * 1852)
            self.assertAlmostEqual(own['calc']['rate_of_closure_mps'], 600 * 1852 / 3600)
            self.assertEqual(own['calc']['los_down_m'], -1852)
            self.assertEqual(own['calc']['radar_range_m'], 40 * 1852)

    def test_metric_and_imperial_records_describe_same_flight(self):
        def read(header, values):
            with tempfile.TemporaryDirectory() as directory:
                path = Path(directory) / 'F-16 (Test--blue) [Blue].csv'
                with path.open('w', encoding='utf-8', newline='') as stream:
                    writer = csv.writer(stream)
                    writer.writerow(header)
                    writer.writerow(values)
                before = path.read_bytes()
                result = read_recording(Path(directory))[0][0]
                self.assertEqual(path.read_bytes(), before)
                return result
        metric = read(['time_s', 'lon_deg', 'lat_deg', 'alt_m', 'tas_fps'],
                      [1, 18, 59, 3048, 500 * 1852 / 3600 / .3048])
        imperial = read(['time_s', 'lon_deg', 'lat_deg', 'alt_ft', 'tas_kt'],
                        [1, 18, 59, 10000, 500])
        for key in ('latitude_deg', 'longitude_deg', 'altitude_m', 'true_airspeed_mps', 'east_m', 'north_m'):
            self.assertAlmostEqual(metric[key], imperial[key], msg=key)

    def test_sparse_imperial_samples_are_not_converted_twice(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'F-16 (Test--blue) [Blue].csv'
            path.write_text('time_s,lon_deg,lat_deg,alt_ft,tas_kt\n'
                            '0,18,59,10000,500\n1,,,,\n2,,,11000,\n', encoding='utf-8')
            samples, _ = read_recording(Path(directory))
            self.assertEqual([sample['altitude_m'] for sample in samples], [3048, 3048, 3352.8])
            for sample in samples:
                self.assertAlmostEqual(sample['true_airspeed_mps'], 500 * 1852 / 3600)


if __name__ == '__main__':
    unittest.main()
