import math
from pathlib import Path
import tempfile
import unittest
from recordings import ecef, geodetic_latitude, metadata, RecordingStore

HEADER='Time,Longitude,Latitude,Altitude,Roll (deg),Pitch (deg),Yaw (deg)\n'

class ReviewFixes(unittest.TestCase):
    def test_geodetic_coordinate_matches_jsbsim_radius(self):
        # Independent values sampled from local JSBSim 1.3.1 at lat-gc=59°, h-sl=5000 m.
        # ecef() now takes geodetic latitude, so the geocentric value has to be
        # converted first; the point must land back on the same radius and angle.
        self.assertAlmostEqual(geodetic_latitude(59,5000),59.16949951946566,places=7)
        xyz=ecef(geodetic_latitude(59,5000),18,5000)
        self.assertAlmostEqual(math.degrees(math.atan2(xyz[2],math.hypot(*xyz[:2]))),59,places=7)
        # h-sl is the ellipsoid height to within 1.3 cm, which is the residual here.
        self.assertAlmostEqual(math.dist(xyz,(0,0,0)),20890432.865796573*.3048,delta=.05)

    def test_poles_equator_and_radial_climb(self):
        for lat in (-90,-59,0,59,90):
            self.assertAlmostEqual(math.dist(ecef(lat,18,5000),ecef(lat,18,5010)),10,places=7)
            self.assertTrue(all(math.isfinite(v) for v in ecef(lat,180,10000)))

    def test_other_missiles_and_models_are_not_f16(self):
        for model in ('R-27','Meteor','MICA','PL-15'):
            item=metadata(Path(model+' (M1) [Red].csv'))
            self.assertEqual(item['kind'],'missile')
            self.assertEqual(item['visual_model'],'generic')
        self.assertEqual(metadata(Path('MiG-29 [Red].csv'))['visual_model'],'generic')
        self.assertEqual(metadata(Path('F-16 [Blue].csv'))['visual_model'],'f16')
        self.assertEqual(metadata(Path('F-16.csv'))['team'],'unknown')

    def test_different_library_does_not_change_active_recording(self):
        with tempfile.TemporaryDirectory() as tmp:
            base=Path(tmp);first=base/'one';second=base/'two';first.mkdir();second.mkdir()
            (first/'F-16 [Blue].csv').write_text(HEADER+'0,18,59,5000,0,0,90\n1,18,59,5010,0,0,90',encoding='utf-8')
            store=RecordingStore(first,settle_seconds=0);store.refresh()
            session=store.session;store.change_directory(str(second))
            self.assertEqual(store.session,session)
            self.assertEqual(store.catalog(),[])
            self.assertEqual(store.packet()['recording'],str(first.resolve()))

    def test_replacement_waits_and_bad_snapshot_preserves_name_and_session(self):
        with tempfile.TemporaryDirectory() as tmp:
            base=Path(tmp);good=base/'good';bad=base/'bad';good.mkdir();bad.mkdir()
            (good/'F-16 [Blue].csv').write_text(HEADER+'0,18,59,5000,0,0,90',encoding='utf-8')
            (bad/'F-16 [Blue].csv').write_text('Time\n0',encoding='utf-8')
            store=RecordingStore(good,settle_seconds=2);store.refresh(0);store.refresh(2.1)
            session=store.session;store.change_directory(str(base))
            store.open_catalog(next(item['id'] for item in store.catalog() if item['name']=='bad'))
            self.assertEqual(store.phase,'waiting');self.assertEqual(store.session,session)
            store.refresh(store.pending_since+2.1)
            self.assertEqual(store.phase,'error');self.assertEqual(store.session,session)
            self.assertEqual(store.packet()['recording_name'],'good')
            self.assertEqual(store.packet()['recording'],str(good.resolve()))

if __name__=='__main__':unittest.main()
