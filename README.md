# cheesestrike
Cheese Strike: Gouda Overload

## License

Copyright (c) 2026 Hamid Golgiri. All rights reserved. The source is public for reference only; see [LICENSE](LICENSE).

## Development

The game is a [Godot 4.7](https://godotengine.org) project in `game/`. Web builds connect players peer-to-peer over WebRTC; `signaling/` is the small server that lists sessions and brokers those connections.

You need Godot 4.7 with the Web export templates installed, and Python 3.10+ with `pip install -r signaling/requirements.txt`.

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

Then open http://localhost:8060 in two browser windows, host in one and join from the other. Browsers pause the game in a background tab. If that tab is hosting, the other players get a "Host paused" notice and are frozen until it comes back, so keep the host's window visible. A phone on the same network can join at `http://<this PC's address>:8060`.
