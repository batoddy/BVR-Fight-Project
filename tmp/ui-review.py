"""Local UI review via Chrome DevTools; uses only the Python standard library."""
import base64
import json
import os
from pathlib import Path
import socket
import struct
import subprocess
import sys
import time
import urllib.request

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'tmp' / 'ui-review'
OUT.mkdir(exist_ok=True)


def free_port():
    with socket.socket() as sock:
        sock.bind(('127.0.0.1', 0))
        return sock.getsockname()[1]


def get(url):
    with urllib.request.urlopen(url, timeout=2) as response:
        return json.load(response)


def until(fn, timeout=25):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        try:
            value = fn()
            if value:
                return value
        except (OSError, ValueError):
            pass
        time.sleep(.15)
    raise TimeoutError('Timed out waiting for browser or viewer')


class CDP:
    def __init__(self, url):
        from urllib.parse import urlsplit
        address = urlsplit(url)
        self.sock = socket.create_connection((address.hostname, address.port), timeout=25)
        key = base64.b64encode(os.urandom(16)).decode()
        self.sock.sendall((f'GET {address.path} HTTP/1.1\r\nHost: {address.netloc}\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Key: {key}\r\nSec-WebSocket-Version: 13\r\n\r\n').encode())
        header = b''
        while not header.endswith(b'\r\n\r\n'):
            header += self.sock.recv(1)
        assert header.startswith(b'HTTP/1.1 101 '), header
        self.sequence = 0

    def read(self, count):
        data = b''
        while len(data) < count:
            chunk = self.sock.recv(count - len(data))
            if not chunk:
                raise EOFError('Chrome closed the connection')
            data += chunk
        return data

    def call(self, method, **params):
        self.sequence += 1
        raw = json.dumps(dict(id=self.sequence, method=method, params=params)).encode()
        mask = os.urandom(4)
        length = len(raw)
        header = bytes([0x81, 0x80 | (length if length < 126 else 126 if length < 65536 else 127)])
        if length >= 126:
            header += struct.pack('!H' if length < 65536 else '!Q', length)
        self.sock.sendall(header + mask + bytes(value ^ mask[i % 4] for i, value in enumerate(raw)))
        while True:
            first, second = self.read(2)
            size = second & 127
            if size == 126:
                size = struct.unpack('!H', self.read(2))[0]
            elif size == 127:
                size = struct.unpack('!Q', self.read(8))[0]
            payload = self.read(size)
            if first & 15 != 1:
                continue
            response = json.loads(payload)
            if response.get('id') == self.sequence:
                if 'error' in response:
                    raise RuntimeError(response['error'])
                return response.get('result', {})

    def js(self, expression):
        result = self.call('Runtime.evaluate', expression=expression, awaitPromise=True, returnByValue=True)
        if 'exceptionDetails' in result:
            raise AssertionError(result['exceptionDetails'])
        return result.get('result', {}).get('value')

    def screenshot(self, name):
        result = self.call('Page.captureScreenshot', format='png')
        (OUT / name).write_bytes(base64.b64decode(result['data']))


fixture = OUT / 'flight'
fixture.mkdir(exist_ok=True)
for name, lon, heading in [('Blue--blue', 18, 45), ('Red--red', 18.08, 225)]:
    team = 'Blue' if name.startswith('Blue') else 'Red'
    (fixture / f'F-16 ({name}) [{team}].csv').write_text(
        'time_s,lon_deg,lat_deg,alt_m,roll_deg,pitch_deg,yaw_deg,mach\n'
        + ''.join(f'{t},{lon+t*.0001},59,7000,0,0,{heading},0.8\n' for t in range(61)), encoding='utf-8')
(fixture / 'AIM-120 AMRAAM (Blue--blue--0) [Blue].csv').write_text(
    'time_s,lon_deg,lat_deg,alt_m,roll_deg,pitch_deg,yaw_deg,mach\n10,18.001,59,7000,0,0,45,1.8\n30,18.05,59,7100,0,0,45,2.5\n', encoding='utf-8')
(fixture / 'manifest.json').write_text(json.dumps({'coordinates': {'latitude_reference': 'WGS84 geodetic', 'altitude_reference': 'WGS84 ellipsoid height'}}), encoding='utf-8')
(fixture / 'calculations.csv').write_text(
    'time_s,id,target_id,wez_min_range_m,wez_max_range_m,range_3d_m,phase\n'
    + ''.join(f'{t},blue,red,2000,31000,9100,4\n' for t in range(61)), encoding='utf-8')
port, debug_port = free_port(), free_port()
flags = subprocess.CREATE_NO_WINDOW if os.name == 'nt' else 0
server = subprocess.Popen([sys.executable, '-B', str(ROOT / 'BVRGym-Goruntuleyici/viewer.py'), '--recording', str(fixture), '--port', str(port)], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, creationflags=flags)
chrome = None
try:
    until(lambda: get(f'http://127.0.0.1:{port}/api/state').get('phase') == 'ready')
    chrome = subprocess.Popen([r'C:\Program Files\Google\Chrome\Application\chrome.exe', '--headless=new', f'--remote-debugging-port={debug_port}', f'--user-data-dir={OUT / ("profile-" + str(debug_port))}', '--no-first-run', '--no-default-browser-check', '--use-angle=swiftshader', '--enable-unsafe-swiftshader', '--enable-logging=stderr', 'about:blank'], stdout=subprocess.DEVNULL, stderr=(OUT / 'chrome-stderr.log').open('w'), creationflags=flags)
    tabs = until(lambda: get(f'http://127.0.0.1:{debug_port}/json'))
    cdp = CDP(next(tab['webSocketDebuggerUrl'] for tab in tabs if tab['type'] == 'page'))
    cdp.call('Page.enable')
    cdp.call('Runtime.enable')
    cdp.call('Emulation.setDeviceMetricsOverride', width=1440, height=900, deviceScaleFactor=1, mobile=False)
    cdp.call('Page.addScriptToEvaluateOnNewDocument', source='window.reviewErrors=[];window.addEventListener("error",e=>reviewErrors.push(e.message));window.addEventListener("unhandledrejection",e=>reviewErrors.push(String(e.reason)));')
    cdp.call('Page.navigate', url=f'http://127.0.0.1:{port}')
    until(lambda: cdp.js('document.querySelector("#timeline") && !document.querySelector("#timeline").disabled'))
    cdp.js('document.querySelector("#seek-time").value="20";document.querySelector("#seek-time").dispatchEvent(new Event("change"));')
    time.sleep(.5)
    assert cdp.js('document.querySelector("#altitude").closest("div").querySelector("dt").textContent') == 'İrtifa · WGS84'
    assert cdp.js('document.querySelector("#calc-wez").textContent') == '16,74 NM'
    assert cdp.js('[...document.querySelectorAll("#object-scale option")].map(o=>o.value)') == ['1', '3']
    assert cdp.js('document.querySelector(".toolbar-actions > #follow") === null && document.querySelector("#toggle-altitude-guides") === null')
    assert cdp.js('(async()=>{const {scene}=await import("/app.js");return !!scene.models})()'), '3D initialization failed'
    model_check = cdp.js('''(async()=>{const {createF16,createMissile}=await import('/aircraft3d.js');const {visualScale}=await import('/units.js');const THREE=await import('three');return ['aircraft','missile'].map(kind=>{const model=kind==='aircraft'?createF16():createMissile();const box=new THREE.Box3().setFromObject(model);let triangles=0;model.traverse(n=>{if(n.geometry){const a=n.geometry.attributes.position.array;if(![...a].every(Number.isFinite))throw Error('Non-finite geometry');triangles+=(n.geometry.index?.count||n.geometry.attributes.position.count)/3;}});return {kind,length:box.max.z-box.min.z,triangles,scale:visualScale(kind,3)};});})()''')
    assert abs(model_check[0]['length'] - 15.06) < .001
    assert abs(model_check[1]['length'] - 3.66) < .001
    assert model_check[1]['scale'] == 11.25
    cdp.js('document.querySelector("#play-pause").click()')
    assert cdp.js('!document.querySelector("#pause-icon").hidden && document.querySelector("#play-icon").hidden')
    cdp.js('document.querySelector("#play-pause").click();document.querySelector("#view-2d").click()')
    time.sleep(.4)
    cdp.screenshot('2d-dark.png')
    hit_check = cdp.js('''(async()=>{const {scene}=await import('/app.js');scene.follow=false;const flat=scene.flat;const position=flat.positions.find(p=>p.label);const rect=flat.canvas.getBoundingClientRect();const event={clientX:rect.left+position.label.right-3,clientY:rect.top+position.label.y+4};if(flat.hitTest(event)?.id!==position.id)throw Error('Label edge selection failed');const saved=flat.positions;flat.positions=[{id:'far',x:100,y:100},{id:'near',x:110,y:100}];if(flat.hitTest({clientX:rect.left+111,clientY:rect.top+100})?.id!=='near')throw Error('Nearest selection failed');flat.positions=saved;return true;})()''')
    assert hit_check
    cdp.js('document.querySelector("#object-scale").value="3";document.querySelector("#object-scale").dispatchEvent(new Event("change"));')
    cdp.js('''(async()=>{const {scene,buffer}=await import('/app.js');const e=buffer.entities(20).find(e=>e.kind==='aircraft');scene.flat.focusEntity(e);})()''')
    time.sleep(.4)
    cdp.screenshot('2d-close.png')
    cdp.js('document.querySelector("#toggle-theme").click()')
    time.sleep(.2)
    cdp.screenshot('2d-light.png')
    cdp.js('document.querySelector("#view-3d").click();document.querySelector("#toggle-theme").click()')
    cdp.js('''(async()=>{const {scene,buffer}=await import('/app.js');scene.focusEntity(buffer.entities(20).find(e=>e.kind==='aircraft'));})()''')
    time.sleep(.6)
    cdp.screenshot('3d-close.png')
    cdp.js('document.querySelector("#view-options").open=true')
    time.sleep(.2)
    assert cdp.js('document.querySelector("#follow").checkVisibility()')
    cdp.js('document.querySelector("#follow").click()')
    assert cdp.js('(async()=>{const {scene}=await import("/app.js");return !scene.follow})()')
    cdp.js('document.querySelector("#view-options").open=false')
    for width, height in [(900,700), (390,844)]:
        cdp.call('Emulation.setDeviceMetricsOverride', width=width, height=height, deviceScaleFactor=1, mobile=False)
        time.sleep(.3)
        assert cdp.js('document.documentElement.scrollWidth<=innerWidth'), 'Horizontal page overflow'
    cdp.screenshot('mobile.png')
    assert not cdp.js('window.reviewErrors'), cdp.js('window.reviewErrors')
    print(json.dumps({'passed':True,'checks':['3D initialization','finite model geometry','normalized lengths','missile boost preserved','1x/3x toolbar','follow menu','pause icon','max WEZ','altitude reference','2D label hit area','nearest object','responsive layout','no browser errors'],'models':model_check,'screenshots':str(OUT)}, ensure_ascii=False), flush=True)
finally:
    if chrome:
        chrome.terminate()
        chrome.wait(timeout=15)
    server.terminate()
    server.wait(timeout=15)
