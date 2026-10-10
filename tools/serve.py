"""Serves the web build for local testing.

Caching is turned off so that reloading the page always picks up the latest
export; browsers otherwise keep running a stale copy of the game data.
"""

import functools
import http.server
import pathlib
import sys

WEB_BUILD = pathlib.Path(__file__).resolve().parent.parent / "build" / "web"


class Handler(http.server.SimpleHTTPRequestHandler):
    def end_headers(self):
        self.send_header("Cache-Control", "no-store")
        super().end_headers()


if __name__ == "__main__":
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 8060
    handler = functools.partial(Handler, directory=str(WEB_BUILD))
    print(f"serving {WEB_BUILD} on http://localhost:{port}", flush=True)
    http.server.ThreadingHTTPServer(("", port), handler).serve_forever()
