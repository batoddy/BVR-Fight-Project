import math
from pathlib import Path
import sys
import tempfile
import unittest
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from recordings import read_exports, RecordingStore
from telemetry import standard_temperature

HEADER='Time,Longitude,Latitude,Altitude,Roll (deg),Pitch (deg),Yaw (deg)'
AIR='F-16 (Blue) [Blue].csv'
MISSILE='AIM-120 AMRAAM (AIM0) [Blue].csv'
def file(name, rows, header=HEADER):
    return dict(name=name,text=header+'\n'+'\n'.join(rows))

class TelemetryTests(unittest.TestCase):
    def test_speed_uses_true_elapsed_time_and_initial_sample_is_not_false_zero(self):
        samples,_=read_exports([file(AIR,['0,18,59,5000,0,0,90','0.2,18,59,5002,0,0,90','1.7,18,59,5017,0,0,90'])])
        for row in samples:
            self.assertAlmostEqual(row['estimated_tas_mps'],10,places=6)
            self.assertIsNone(row['true_airspeed_mps'])
            self.assertIsNone(row['mach'])
            self.assertGreater(row['estimated_mach'],0)

    def test_recorded_tas_and_mach_take_precedence(self):
        samples,_=read_exports([file(AIR,['0,18,59,5000,0,0,90,250,0.8','2,18.01,59,5000,0,0,90,260,0.9'],HEADER+',TAS,Mach')])
        self.assertEqual([row['true_airspeed_mps'] for row in samples],[250,260])
        self.assertEqual([row['mach'] for row in samples],[.8,.9])
        self.assertTrue(all(row['estimated_tas_mps'] is None and row['estimated_mach'] is None for row in samples))

    def test_single_sample_stays_unknown_and_stationary_track_is_zero(self):
        samples,_=read_exports([file(AIR,['0,18,59,5000,0,0,90'])])
        self.assertIsNone(samples[0]['estimated_tas_mps'])
        samples,_=read_exports([file(AIR,['0,18,59,5000,0,0,90','1,18,59,5000,0,0,90'])])
        self.assertEqual(samples[0]['estimated_tas_mps'],0)
        self.assertEqual(samples[1]['estimated_mach'],0)

    def test_inventory_preserves_unfired_files_without_spawning_them(self):
        exports=[file(AIR,['0,18,59,5000,0,0,90','10,18.01,59,5000,0,0,90']),
                 file(MISSILE,['3,18,59,5000,0,0,90','5,18.01,59,5000,0,0,90']),
                 dict(name='AIM-120 AMRAAM (AIM1) [Blue].csv',text='\n')]
        samples,entities=read_exports(exports)
        self.assertEqual(len(entities),2)
        inventory=next(entity for entity in entities if entity['kind']=='aircraft')['inventory']
        self.assertEqual(len(inventory),2)
        self.assertEqual(inventory[0]['owner_id'],AIR)
        self.assertEqual((inventory[0]['record_start_s'],inventory[0]['record_end_s']),(3,5))
        self.assertFalse(inventory[1]['has_track'])
        self.assertTrue(all(row['id']!=inventory[1]['id'] for row in samples))
        with tempfile.TemporaryDirectory() as temporary:
            store=RecordingStore(temporary);store.import_files(exports)
            self.assertEqual(len(store.packet()['inventory'][AIR]),2)

    def test_multiple_same_team_aircraft_do_not_get_guessed_ownership(self):
        exports=[file(AIR,['0,18,59,5000,0,0,90']),
                 file('F-16 (Wingman) [Blue].csv',['0,18.1,59,5000,0,0,90']),
                 dict(name=MISSILE,text='\n')]
        _,entities=read_exports(exports)
        self.assertTrue(all(not entity['inventory'] for entity in entities if entity['kind']=='aircraft'))
        self.assertIsNone(entities[0]['unassigned_inventory'][0]['owner_id'])

    def test_standard_atmosphere_boundaries_and_limits(self):
        self.assertAlmostEqual(standard_temperature(11000),216.65)
        self.assertAlmostEqual(standard_temperature(20000),216.65)
        self.assertAlmostEqual(standard_temperature(32000),228.65)
        self.assertIsNone(standard_temperature(90000))

if __name__=='__main__':unittest.main()
