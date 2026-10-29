"""Local recording viewer. Directory selection changes the viewer, never the simulation."""
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
import csv
from pathlib import Path
from native_picker import choose_directory
from urllib.parse import urlsplit, parse_qs

WEB = Path(__file__).parent / "web"
FILES = {"/": ("index.html", "text/html; charset=utf-8"),
         "/styles.css": ("styles.css", "text/css; charset=utf-8"),
         "/app.js": ("app.js", "text/javascript; charset=utf-8"),
         "/scene.js": ("scene.js", "text/javascript; charset=utf-8"),
         "/scene2d.js": ("scene2d.js", "text/javascript; charset=utf-8"),
         "/aircraft3d.js": ("aircraft3d.js", "text/javascript; charset=utf-8"),
         "/vendor/three.module.js": ("vendor/three.module.js", "text/javascript; charset=utf-8"),
         "/vendor/three.core.js": ("vendor/three.core.js", "text/javascript; charset=utf-8"),
         "/vendor/OrbitControls.js": ("vendor/OrbitControls.js", "text/javascript; charset=utf-8"),
         "/stream.js": ("stream.js", "text/javascript; charset=utf-8")}
FILES['/recording-menu.js'] = ('recording-menu.js', 'text/javascript; charset=utf-8')
FILES['/units.js'] = ('units.js', 'text/javascript; charset=utf-8')
FILES['/inventory.js'] = ('inventory.js', 'text/javascript; charset=utf-8')
FILES['/basemap.js'] = ('basemap.js', 'text/javascript; charset=utf-8')
FILES['/ruler.js'] = ('ruler.js', 'text/javascript; charset=utf-8')


def make_server(store, port=8765, directory_picker=choose_directory):
    class Handler(BaseHTTPRequestHandler):
        def log_message(self, *_):
            pass

        def send_data(self, status, content, mime):
            self.send_response(status)
            self.send_header("Content-Type", mime)
            self.send_header("Content-Length", str(len(content)))
            self.send_header("Cache-Control", "no-store")
            self.send_header("X-Content-Type-Options", "nosniff")
            self.end_headers()
            self.wfile.write(content)

        def do_GET(self):
            request = urlsplit(self.path)
            if request.path == '/api/recordings':
                self.send_data(200, json.dumps(store.catalog(), ensure_ascii=False).encode('utf-8'), 'application/json; charset=utf-8')
            elif request.path == "/api/state":
                try:
                    after = int(parse_qs(request.query).get("after", ["0"])[0])
                    if after < 0:
                        raise ValueError()
                except ValueError:
                    self.send_error(400, "Invalid cursor")
                    return
                session = parse_qs(request.query, keep_blank_values=True).get('session', [None])[0]
                data = json.dumps(store.packet(after, session=session), ensure_ascii=False,
                                  allow_nan=False).encode("utf-8")
                self.send_data(200, data, "application/json; charset=utf-8")
            elif request.path in FILES:
                filename, mime = FILES[request.path]
                self.send_data(200, (WEB / filename).read_bytes(), mime)
            else:
                self.send_error(404)

        def do_POST(self):
            request = urlsplit(self.path)
            if request.path not in ('/api/directory', '/api/choose-directory', '/api/open', '/api/delete'):
                self.send_error(405, 'No flight-control endpoints')
                return
            origin = self.headers.get('Origin')
            if origin and origin != 'http://' + self.headers.get('Host', ''):
                self.send_error(403, 'Same-origin requests required')
                return
            if self.headers.get('Content-Type', '').split(';')[0] != 'application/json':
                self.send_error(415, 'JSON required')
                return
            try:
                length = int(self.headers.get('Content-Length', '0'))
                if not 0 < length <= 64 * 1024:
                    self.send_data(413, json.dumps({'error': 'İstek çok büyük'}, ensure_ascii=False).encode('utf-8'), 'application/json; charset=utf-8')
                    return
                payload = json.loads(self.rfile.read(length).decode('utf-8'))
                if not isinstance(payload, dict):
                    raise ValueError('Geçersiz istek')
                if request.path == '/api/choose-directory':
                    selected = directory_picker(store.catalog_root)
                    if selected:
                        store.change_directory(selected)
                    self.send_data(200, json.dumps({'cancelled': not bool(selected), 'path': str(store.catalog_root)}, ensure_ascii=False).encode('utf-8'), 'application/json; charset=utf-8')
                    return
                elif request.path == '/api/directory':
                    store.change_directory(payload.get('path'))
                elif request.path == '/api/delete':
                    store.delete_catalog(payload.get('id'))
                else:
                    store.open_catalog(payload.get('id'))
            except (ValueError, OSError, csv.Error) as exc:
                self.send_data(400, json.dumps({'error': str(exc)}, ensure_ascii=False).encode('utf-8'), 'application/json; charset=utf-8')
                return
            self.send_data(200, b'{"ok":true}', 'application/json; charset=utf-8')

    return ThreadingHTTPServer(("127.0.0.1", port), Handler)
