"""What the browser tests share: the servers the game needs, a headless
browser, and a wrapper for one browser window running the game.

A test plays the game the way a person does, by clicking and typing in real
browser windows that connect to each other over WebRTC. It can't see the game's
canvas, so it reads what is on screen from `window.cheeseStrikeTest`, which the
game publishes when opened with `?test` (see game/test_hook.gd).

The web build under test is the folder named by CHEESESTRIKE_WEB_BUILD, or
build/browser-tests. tools/run_tests.py exports it before running these.
"""

import functools
import http.server
import os
import pathlib
import socket
import subprocess
import sys
import threading
import unittest

from playwright.sync_api import sync_playwright

ROOT = pathlib.Path(__file__).resolve().parent.parent
WEB_BUILD = pathlib.Path(os.environ.get("CHEESESTRIKE_WEB_BUILD", ROOT / "build" / "browser-tests"))

# Generous, for a slow CI machine. Waits end as soon as their condition holds,
# so this costs nothing on success.
TIMEOUT_MS = 30_000

VIEWPORT = {"width": 640, "height": 400}


class _QuietHandler(http.server.SimpleHTTPRequestHandler):
    def log_message(self, *args):
        pass


def _free_port():
    with socket.socket() as probe:
        probe.bind(("127.0.0.1", 0))
        return probe.getsockname()[1]


class SignalingServer:
    """The real signaling server, as its own process on a port of its own."""

    def __init__(self):
        self.port = _free_port()
        self._process = None

    def start(self):
        self._process = subprocess.Popen(
            [sys.executable, "-u", str(ROOT / "signaling" / "server.py"), "--host", "127.0.0.1", "--port", str(self.port)],
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
        )
        line = self._process.stdout.readline()
        if "listening" not in line:
            raise RuntimeError(f"signaling server didn't start: {line}{self._process.stdout.read()}")
        # Keep reading so the server never blocks on a full pipe.
        threading.Thread(target=self._process.stdout.read, daemon=True).start()

    def stop(self):
        if self._process is not None:
            self._process.kill()
            self._process.wait()
            self._process.stdout.close()
            self._process = None


class Game:
    """One browser window running the game."""

    def __init__(self, page, url):
        self.page = page
        self.errors = []
        page.on("pageerror", lambda error: self.errors.append(str(error)))
        page.goto(url)
        self.wait_for("s.screen === 'menu'")

    def state(self):
        return self.page.evaluate("window.cheeseStrikeTest")

    def wait_for(self, condition, timeout=TIMEOUT_MS):
        """Waits until `condition`, a JavaScript expression over the published
        state `s`, is true, and returns the state at that moment."""
        try:
            handle = self.page.wait_for_function(
                f"(() => {{ const s = window.cheeseStrikeTest; return s && ({condition}) ? s : null; }})()",
                timeout=timeout,
                polling=50,
            )
        except Exception as error:
            raise AssertionError(f"never became true: {condition}\nstate: {self._state_or_none()}\n{error}") from None
        return handle.json_value()

    def click(self, control):
        """Clicks a control by its node name in the game, e.g. "HostButton"."""
        # A control that has just appeared is reported once before the game
        # has laid it out, so wait for it to stop moving.
        at = None
        while True:
            state = self.wait_for(f"'{control}' in s.controls")
            if state["controls"][control] == at:
                break
            at = state["controls"][control]
            self.page.wait_for_timeout(100)
        self.click_at(at)

    def click_at(self, canvas_point):
        """Clicks a point given in the game canvas's own pixels."""
        x, y = self.page.evaluate(
            """([x, y]) => {
                const canvas = document.querySelector('canvas');
                const box = canvas.getBoundingClientRect();
                return [box.left + x * box.width / canvas.width, box.top + y * box.height / canvas.height];
            }""",
            canvas_point,
        )
        self.page.mouse.click(x, y)

    def type(self, text):
        self.page.keyboard.type(text, delay=30)

    def press(self, key):
        self.page.keyboard.press(key)

    def host(self):
        """Hosts a session and returns its code."""
        self.click("HostButton")
        return self.wait_for("s.screen === 'game' && s.code")["code"]

    def join(self, code):
        self.click("TargetEdit")
        self.type(code)
        self.press("Enter")
        self.wait_for("s.screen === 'game'")

    def own_player(self):
        """This window's own character, as published: id, mine, position."""
        state = self.wait_for("s.players.some((player) => player.mine)")
        return next(player for player in state["players"] if player["mine"])

    def close(self):
        self.page.close()

    def _state_or_none(self):
        try:
            return self.state()
        except Exception:
            return None


class GameTestCase(unittest.TestCase):
    """Gives each test a signaling server with no sessions on it and fresh
    browser windows, and fails a test whose page raised a JavaScript error."""

    @classmethod
    def setUpClass(cls):
        if not (WEB_BUILD / "index.html").exists():
            raise RuntimeError(f"no web build at {WEB_BUILD}; run: python tools/run_tests.py browser")
        handler = functools.partial(_QuietHandler, directory=str(WEB_BUILD))
        cls._web = http.server.ThreadingHTTPServer(("127.0.0.1", 0), handler)
        threading.Thread(target=cls._web.serve_forever, daemon=True).start()
        cls._playwright = sync_playwright().start()
        cls._browser = cls._playwright.chromium.launch(
            # The game needs WebGL to start, and CI has no graphics card, so
            # use the software renderer. The game doesn't draw under test.
            args=["--enable-unsafe-swiftshader", "--use-angle=swiftshader"]
        )
        # One context for the class, so the engine download is cached between tests.
        cls._context = cls._browser.new_context(viewport=VIEWPORT)

    @classmethod
    def tearDownClass(cls):
        cls._context.close()
        cls._browser.close()
        cls._playwright.stop()
        cls._web.shutdown()
        cls._web.server_close()

    def setUp(self):
        self.signaling = SignalingServer()
        self.signaling.start()
        self._games = []

    def tearDown(self):
        errors = [error for game in self._games for error in game.errors]
        for game in self._games:
            if not game.page.is_closed():
                game.close()
        self.signaling.stop()
        self.assertEqual(errors, [], "a page raised a JavaScript error")

    def open_game(self):
        """Opens the game in a new window and waits for its menu."""
        url = f"http://localhost:{self._web.server_address[1]}/?test&signal=ws://localhost:{self.signaling.port}"
        game = Game(self._context.new_page(), url)
        self._games.append(game)
        return game
