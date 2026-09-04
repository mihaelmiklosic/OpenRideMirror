from __future__ import annotations

import json
import secrets
import threading
import webbrowser
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from typing import Any

from .map_builder import build
from .paths import repo_root


def serve_directory(directory: Path, port: int, open_browser: bool = True) -> None:
    class Handler(SimpleHTTPRequestHandler):
        def __init__(self, *args: Any, **kwargs: Any) -> None:
            super().__init__(*args, directory=str(directory), **kwargs)

    server = ThreadingHTTPServer(("127.0.0.1", port), Handler)
    if open_browser:
        threading.Timer(.3, lambda: webbrowser.open(f"http://127.0.0.1:{port}/")).start()
    print(f"Serving {directory} at http://127.0.0.1:{port}/ (Ctrl-C to stop)")
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()


def serve_map_ui(config: dict[str, Any], port: int = 8767, open_browser: bool = True) -> None:
    directory = Path(__file__).resolve().parent / "web"
    token = secrets.token_urlsafe(24)

    class Handler(SimpleHTTPRequestHandler):
        def __init__(self, *args: Any, **kwargs: Any) -> None:
            super().__init__(*args, directory=str(directory), **kwargs)

        def end_headers(self) -> None:
            self.send_header("X-Content-Type-Options", "nosniff")
            self.send_header("Content-Security-Policy", "default-src 'self'; script-src 'self'; style-src 'self'; connect-src 'self'")
            super().end_headers()

        def do_GET(self) -> None:
            if self.path == "/api/session":
                self.reply({"token": token, "config": config.get("map", {})})
                return
            if self.path == "/api/preview":
                preview = repo_root() / ".orm" / "generated" / "map" / "map-preview.json"
                if not preview.exists():
                    self.send_error(404)
                    return
                self.send_response(200)
                self.send_header("Content-Type", "application/json")
                self.end_headers()
                self.wfile.write(preview.read_bytes())
                return
            super().do_GET()

        def do_POST(self) -> None:
            if self.path != "/api/build" or self.headers.get("X-ORM-Token") != token:
                self.send_error(403)
                return
            origin = self.headers.get("Origin")
            if origin and origin != f"http://127.0.0.1:{port}":
                self.send_error(403)
                return
            length = min(int(self.headers.get("Content-Length", "0")), 16384)
            try:
                request = json.loads(self.rfile.read(length))
                temporary = {**config, "map": request}
                output, manifest = build(temporary, bool(request.get("offline", False)))
                self.reply({"ok": True, "output": str(output), "manifest": manifest})
            except Exception as error:
                self.send_response(400)
                self.send_header("Content-Type", "application/json")
                self.end_headers()
                self.wfile.write(json.dumps({"ok": False, "error": str(error)}).encode())

        def reply(self, value: Any) -> None:
            payload = json.dumps(value).encode()
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(payload)))
            self.end_headers()
            self.wfile.write(payload)

    server = ThreadingHTTPServer(("127.0.0.1", port), Handler)
    if open_browser:
        threading.Timer(.3, lambda: webbrowser.open(f"http://127.0.0.1:{port}/")).start()
    print(f"ORM map UI: http://127.0.0.1:{port}/ (Ctrl-C to stop)")
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()
