# cheesestrike

[![Tests](https://github.com/golgirihm/cheesestrike/actions/workflows/tests.yml/badge.svg?branch=main&event=push)](https://github.com/golgirihm/cheesestrike/actions/workflows/tests.yml?query=branch%3Amain)

Cheese Strike: Gouda Overload

## License

Copyright (c) 2026 Hamid Golgiri. All rights reserved. The source is public for reference only; see [LICENSE](LICENSE).

## Development

The game is a [Godot 4.7](https://godotengine.org) project in `game/`. Players connect peer-to-peer over WebRTC, in the browser and in native builds alike; `signaling/` is the small server that lists sessions and brokers those connections. In development it runs as one Python process; the public one runs the same rules on AWS with nothing left running between players. [docs/backend.md](docs/backend.md) describes that, with a diagram, and how to deploy it.

You need Godot 4.7 with the Web export templates installed, and Python 3.14 with `pip install -r signaling/requirements.txt`.

Bought and downloaded models, textures and animations are in `game/licensed/`, a submodule from the private repository [cheesestrike-assets](https://github.com/golgirihm/cheesestrike-assets). Their licenses allow them in the released game but forbid sharing their files, so they stay out of this public repository. If you have access, fetch them after cloning:

```
git submodule update --init
```

On Windows, use `godot_console` in place of `godot` in the commands below. Plain `godot` is a windowed program there and prints nothing to the terminal.

Run natively. Native builds connect the same way the browser does, so they need WebRTC, which Godot's desktop builds don't have of their own, and the development server. Install WebRTC once after cloning, and again when its pin in `tools/versions.env` changes:

```
python tools/fetch_webrtc.py
```

Then start the server and two instances, one hosting and one joining the newest session listed:

```
python signaling/local/server.py
godot --path game -- --host
godot --path game -- --join
```

`--join=<code>` joins a session by its code, and `--signal=ws://<address>` uses another session server, such as the one on AWS (its address is in `infra/deployed.json`).

Build and run the web version:

```
mkdir -p build/web
godot --headless --path game --export-debug Web ../build/web/index.html
python tools/serve.py
```

Then open http://localhost:8060 in two browser windows, host in one and join from the other. They find each other through the session server on AWS, whose addresses deploying saves in `infra/deployed.json`. To use the development server instead, as when changing it, run it and serve with `--local`:

```
python signaling/local/server.py
python tools/serve.py --local
```

The automated tests always start a development server of their own, and never reach AWS. A host's tab keeps the session running when it is in the background. If the host stops anyway, as a phone does when its browser is put away, the other players get a "Waiting for host" notice and are frozen until it comes back or they leave. A phone on the same network can join at `http://<this PC's address>:8060`, though phones are deprecated: what works on them is kept, but nothing more is done for them (see [CLAUDE.md](CLAUDE.md#phones-are-deprecated)).

### Tests

There are four suites. Run them all with one command, or name one or more (`game`, `signaling`, `infra`, `browser`) to run only those:

```
python tools/run_tests.py
```

- **Game tests** use [GUT](https://github.com/bitwes/Gut), vendored in `game/addons/gut`, and live in `game/test`.
- **Signaling tests** are in `signaling/`. `test_stores.py` tests the rules on their own, over both places they can keep sessions (memory, and DynamoDB as on AWS); `test_server.py` tests the development server over real connections; `test_aws_lambda.py` tests the function AWS runs. The development server is in `signaling/local/` and the function in `signaling/cloud/`, both running the rules in `signaling/core.py`. **Infra tests**, in `infra/`, check the AWS template, what is uploaded as the function, what deploying does to the game's files, and which server `tools/serve.py` points the game at. Both need `pip install -r signaling/requirements-test.txt`, and use imitations of AWS that run inside the tests: nothing reaches a real account.
- **Browser tests** are in `browser_tests/`. They export the web build, then play it in headless browser windows that host and join sessions with each other for real, using [Playwright](https://playwright.dev/python/). They need a one-time setup:

  ```
  pip install -r browser_tests/requirements.txt
  python -m playwright install --only-shell chromium
  ```

  The game draws to a canvas, which a test can't read, so when the page is opened with `?test` the game publishes what is on screen for the tests to check (`game/test_hook.gd`).

  They take a while, so CI splits them between eight machines: `python tools/run_tests.py browser --shard 2/8` runs the second machine's share, as for reproducing a failure there. Each machine's output ends with how long each of its tests took, slowest first.

The plan for the rest of the game is in [docs/backlog.md](docs/backlog.md).

Behavior that these can't cover, such as input devices, real browsers and phones, is listed as manual tests in [docs/manual-tests.md](docs/manual-tests.md).

CI runs the same script on every pull request and push to `main`, with the versions pinned in `tools/versions.env` and the `requirements.txt` files. A local run uses whatever you have installed, and the script warns when that differs from the pins. It can also be started by hand for any branch from the Tests workflow in the repository's Actions tab, or on a pull request by commenting `buildci`.
