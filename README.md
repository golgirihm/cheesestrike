# cheesestrike

[![Tests](https://github.com/golgirihm/cheesestrike/actions/workflows/tests.yml/badge.svg?branch=main&event=push)](https://github.com/golgirihm/cheesestrike/actions/workflows/tests.yml?query=branch%3Amain)

Cheese Strike: Gouda Overload

## License

Copyright (c) 2026 Hamid Golgiri. All rights reserved. The source is public for reference only; see [LICENSE](LICENSE).

## Development

The game is a [Godot 4.7](https://godotengine.org) project in `game/`. Web builds connect players peer-to-peer over WebRTC; `signaling/` is the small server that lists sessions and brokers those connections.

You need Godot 4.7 with the Web export templates installed, and Python 3.14 with `pip install -r signaling/requirements.txt`.

On Windows, use `godot_console` in place of `godot` in the commands below. Plain `godot` is a windowed program there and prints nothing to the terminal.

Run natively, where host and join use a direct connection on port 7777 and need no signaling server:

```
godot --path game -- --host
godot --path game -- --join=127.0.0.1
```

Build and run the web version:

```
mkdir -p build/web
godot --headless --path game --export-debug Web ../build/web/index.html
python signaling/server.py
python tools/serve.py
```

Then open http://localhost:8060 in two browser windows, host in one and join from the other. A host's tab keeps the session running when it is in the background. If the host stops anyway, as a phone does when its browser is put away, the other players get a "Waiting for host" notice and are frozen until it comes back or they leave. A phone on the same network can join at `http://<this PC's address>:8060`.

### Tests

There are three suites. Run them all with one command, or name one or more (`game`, `signaling`, `browser`) to run only those:

```
python tools/run_tests.py
```

- **Game tests** use [GUT](https://github.com/bitwes/Gut), vendored in `game/addons/gut`, and live in `game/test`.
- **Signaling tests** are in `signaling/test_server.py`.
- **Browser tests** are in `browser_tests/`. They export the web build, then play it in headless browser windows that host and join sessions with each other for real, using [Playwright](https://playwright.dev/python/). They need a one-time setup:

  ```
  pip install -r browser_tests/requirements.txt
  python -m playwright install --only-shell chromium
  ```

  The game draws to a canvas, which a test can't read, so when the page is opened with `?test` the game publishes what is on screen for the tests to check (`game/test_hook.gd`).

Behavior that these can't cover, such as input devices, real browsers and phones, is listed as manual tests in [docs/manual-tests.md](docs/manual-tests.md).

CI runs the same script on every pull request and push to `main`, with the versions pinned in `tools/versions.env` and the `requirements.txt` files. A local run uses whatever you have installed, and the script warns when that differs from the pins. It can also be started by hand for any branch from the Tests workflow in the repository's Actions tab, or on a pull request by commenting `buildci`.
