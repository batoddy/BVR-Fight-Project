"""Local read-only HTTP interface. No flight-control endpoints."""
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
from pathlib import Path
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


def make_server(store, port=8765):
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
            if request.path == "/api/state":
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
            self.send_error(405, "Read-only viewer")

    return ThreadingHTTPServer(("127.0.0.1", port), Handler)
