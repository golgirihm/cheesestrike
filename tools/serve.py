"""Serves the web build for local testing.

Caching is turned off so that reloading the page always picks up the latest
export; browsers otherwise keep running a stale copy of the game data.

For the same reason the game's own service worker is replaced by one that does
nothing. The real one is what lets a player install the game to their home
screen, by keeping the game's files to start it from; served here, a reload
would run the copy it kept rather than the new export.
"""

import functools
import http.server
import pathlib
import sys

WEB_BUILD = pathlib.Path(__file__).resolve().parent.parent / "build" / "web"


# Takes over from any worker an earlier build left, and keeps nothing.
IDLE_WORKER = b"self.addEventListener('install', () => self.skipWaiting());\nself.addEventListener('activate', (event) => event.waitUntil(self.clients.claim()));\n"


class Handler(http.server.SimpleHTTPRequestHandler):
    def do_GET(self):
        if self.path.split("?")[0].endswith(".service.worker.js"):
            self.send_response(200)
            self.send_header("Content-Type", "text/javascript")
            self.send_header("Content-Length", str(len(IDLE_WORKER)))
            self.end_headers()
            self.wfile.write(IDLE_WORKER)
            return
        super().do_GET()

    def end_headers(self):
        self.send_header("Cache-Control", "no-store")
        super().end_headers()


if __name__ == "__main__":
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 8060
    handler = functools.partial(Handler, directory=str(WEB_BUILD))
    print(f"serving {WEB_BUILD} on http://localhost:{port}", flush=True)
    http.server.ThreadingHTTPServer(("", port), handler).serve_forever()
