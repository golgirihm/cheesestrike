"""Tests for tools/serve.py: which session server the pages it serves use.

    python -m unittest discover -s infra
"""

import json
import pathlib
import re
import sys
import tempfile
import threading
import unittest
import urllib.request

ROOT = pathlib.Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))

import serve  # noqa: E402

PAGE = "<html><head><title>CheeseStrike</title></head><body></body></html>"
BACKEND = {"signal": "wss://abc123.execute-api.us-east-2.amazonaws.com/prod", "sessions": "https://xyz789.lambda-url.us-east-2.on.aws/"}


class ServeTest(unittest.TestCase):
    def setUp(self):
        folder = tempfile.TemporaryDirectory()
        self.addCleanup(folder.cleanup)
        self.folder = pathlib.Path(folder.name)
        (self.folder / "index.html").write_text(PAGE, encoding="utf-8")
        (self.folder / "index.js").write_text("console.log('engine');", encoding="utf-8")

    def start(self, backend):
        server = serve.server(0, self.folder, backend)
        threading.Thread(target=server.serve_forever, daemon=True).start()
        self.addCleanup(server.server_close)
        self.addCleanup(server.shutdown)
        return f"http://localhost:{server.server_address[1]}"

    def get(self, url):
        with urllib.request.urlopen(url, timeout=5) as response:
            return response.read().decode("utf-8")

    def config_in(self, page):
        written = re.search(r"window\.cheeseStrikeConfig = (.*?);</script>", page)
        return json.loads(written.group(1)) if written else None

    def test_the_page_is_given_the_deployed_backend(self):
        address = self.start(BACKEND)
        for path in ("/", "/index.html", "/index.html?test"):
            self.assertEqual(self.config_in(self.get(address + path)), BACKEND, path)

    def test_with_local_the_page_is_served_as_built(self):
        address = self.start(None)
        self.assertEqual(self.get(address + "/index.html"), PAGE)

    def test_other_files_are_served_as_built(self):
        address = self.start(BACKEND)
        self.assertEqual(self.get(address + "/index.js"), "console.log('engine');")

    def test_the_saved_addresses_are_the_backends_two(self):
        backend = serve.deployed_backend()
        self.assertIsNotNone(backend, "infra/deployed.json, which deploying writes")
        self.assertTrue(backend["signal"].startswith("wss://"), backend)
        self.assertTrue(backend["sessions"].startswith("https://"), backend)

    def test_with_nothing_deployed_there_is_no_backend(self):
        self.assertIsNone(serve.deployed_backend(self.folder / "deployed.json"))


if __name__ == "__main__":
    unittest.main()
