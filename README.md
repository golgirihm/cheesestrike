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

Game tests use [GUT](https://github.com/bitwes/Gut), vendored in `game/addons/gut`, and live in `game/test`. Signaling server tests are in `signaling/test_server.py`. Run everything with one command, or name a suite (`game`, `signaling`) to run only that one:

```
python tools/run_tests.py
```

CI runs the same script on every pull request and push to `main`, with the versions pinned in `tools/versions.env` and `signaling/requirements.txt`. A local run uses whatever you have installed, and the script warns when that differs from the pins. It can also be started by hand for any branch from the Tests workflow in the repository's Actions tab, or on a pull request by commenting `buildci`.
