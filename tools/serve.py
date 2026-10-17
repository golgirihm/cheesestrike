"""Serves the web build for local testing.

    python tools/serve.py            on port 8060, using the backend on AWS
    python tools/serve.py --local    using the development server instead
    python tools/serve.py 8070       on another port

The page is given the deployed backend's addresses, kept in
infra/deployed.json, as deploying gives the published page, so windows opened
from here host and join through AWS with no server of their own running. With
--local nothing is written, and the game looks for the development server
(python signaling/local/server.py) on port 9080 of this machine, as it does
whenever it is given no addresses. `?signal=ws://...` in the page's address
still overrides either, which is how the browser tests keep to a server of
their own.

Caching is turned off so that reloading the page always picks up the latest
export; browsers otherwise keep running a stale copy of the game data.

For the same reason the game's own service worker is replaced by one that does
nothing. The real one is what lets a player install the game to their home
screen, by keeping the game's files to start it from; served here, a reload
would run the copy it kept rather than the new export.
"""

import argparse
import functools
import http.server
import json
import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
WEB_BUILD = ROOT / "build" / "web"
DEPLOYED = ROOT / "infra" / "deployed.json"

sys.path.insert(0, str(ROOT / "infra"))

from deploy import with_config  # noqa: E402

# Takes over from any worker an earlier build left, and keeps nothing.
IDLE_WORKER = b"self.addEventListener('install', () => self.skipWaiting());\nself.addEventListener('activate', (event) => event.waitUntil(self.clients.claim()));\n"


class Handler(http.server.SimpleHTTPRequestHandler):
    def __init__(self, *args, backend=None, **kwargs):
        self.backend = backend  # {"signal": ..., "sessions": ...}, or None for the development server
        super().__init__(*args, **kwargs)

    def do_GET(self):
        path = self.path.split("?")[0]
        if path.endswith(".service.worker.js"):
            self.send(IDLE_WORKER, "text/javascript")
            return
        if self.backend and path in ("/", "/index.html"):
            page = (pathlib.Path(self.directory) / "index.html").read_text(encoding="utf-8")
            self.send(with_config(page, self.backend["signal"], self.backend["sessions"]).encode("utf-8"), "text/html")
            return
        super().do_GET()

    def send(self, body, content_type):
        self.send_response(200)
        self.send_header("Content-Type", content_type)
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def end_headers(self):
        self.send_header("Cache-Control", "no-store")
        super().end_headers()


def deployed_backend(path=DEPLOYED):
    """The deployed backend's addresses, or None if it hasn't been deployed."""
    if not path.exists():
        return None
    return json.loads(path.read_text(encoding="utf-8"))


def server(port, folder, backend):
    handler = functools.partial(Handler, directory=str(folder), backend=backend)
    return http.server.ThreadingHTTPServer(("", port), handler)


def main():
    parser = argparse.ArgumentParser(description="Serve the web build for local testing.")
    parser.add_argument("port", nargs="?", type=int, default=8060)
    parser.add_argument("--local", action="store_true", help="use the development server, not the one on AWS")
    args = parser.parse_args()
    backend = None if args.local else deployed_backend()
    print(f"serving {WEB_BUILD} on http://localhost:{args.port}", flush=True)
    if backend:
        print(f"using the session server on AWS, {backend['signal']}", flush=True)
    else:
        why = "" if args.local else f" ({DEPLOYED.relative_to(ROOT).as_posix()} is missing)"
        print(f"using the development server{why}: start it with python signaling/local/server.py", flush=True)
    server(args.port, WEB_BUILD, backend).serve_forever()


if __name__ == "__main__":
    main()
