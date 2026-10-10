import json
from pathlib import Path
import tempfile
import threading
import unittest
from unittest.mock import patch
from urllib.request import Request, urlopen
from urllib.error import HTTPError
from recordings import RecordingStore
from server import make_server
from native_picker import choose_directory

class NativePickerChecks(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory();self.base=Path(self.temp.name)
        self.store=RecordingStore(self.base)
    def tearDown(self):self.temp.cleanup()

    def call(self,picker,origin=None):
        server=make_server(self.store,0,directory_picker=picker)
        thread=threading.Thread(target=server.serve_forever,daemon=True);thread.start()
        try:
            headers={'Content-Type':'application/json'}
            if origin:headers['Origin']=origin
            return json.load(urlopen(Request(f'http://127.0.0.1:{server.server_port}/api/choose-directory',data=b'{}',headers=headers)))
        finally:server.shutdown();server.server_close();thread.join()

    def test_selection_and_cancel_keep_open_flight_session(self):
        selected=self.base/'Kayıtlar';selected.mkdir();session=self.store.session
        with patch('builtins.input',side_effect=AssertionError('No console prompt')):
            result=self.call(lambda initial:str(selected))
        self.assertFalse(result['cancelled']);self.assertEqual(Path(result['path']),selected.resolve())
        self.assertEqual(self.store.session,session)
        result=self.call(lambda initial:None)
        self.assertTrue(result['cancelled']);self.assertEqual(self.store.catalog_root,selected.resolve())

    def test_foreign_origin_never_opens_dialog(self):
        with patch('native_picker.subprocess.run') as runner:
            with self.assertRaises(HTTPError) as caught:self.call(choose_directory,'http://foreign.example')
            self.assertEqual(caught.exception.code,403);runner.assert_not_called()

    def test_dialog_has_no_shell_path_interpolation_and_cancel_is_none(self):
        with patch('native_picker.subprocess.run') as runner:
            runner.return_value.returncode=0;runner.return_value.stdout=''
            self.assertIsNone(choose_directory(r'C:\Kayıtlar $()'))
            positional,options=runner.call_args
            self.assertNotIn(r'C:\Kayıtlar $()',positional[0])
            self.assertEqual(options['env']['BVRGYM_PICKER_INITIAL_DIR'],r'C:\Kayıtlar $()')
            self.assertIn('-STA',positional[0]);self.assertNotIn('shell',options)

if __name__=='__main__':unittest.main()
