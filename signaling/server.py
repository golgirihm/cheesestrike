"""Signaling server for CheeseStrike, as one process: what development and the
tests run. The public one runs the same rules on AWS (signaling/aws_lambda.py).

The rules and the protocol are in signaling/core.py. This carries the
messages: WebSocket connections for hosts and for players joining, and on the
same port two plain HTTP addresses for everyone else:

    GET /sessions          {"sessions": [{"code": "ABCD", "players": 3, "settings": {...}}]}
    GET /sessions/ABCD     {"open": true}    whether that session's host is connected

Limits of its own, on top of the rules': a message may be at most
MAX_MESSAGE_SIZE bytes (a larger one closes the connection), and each address
may send MESSAGE_RATE messages a second on average, in bursts of up to
MESSAGE_BURST. A message over the rate is dropped with an error. Behind a
reverse proxy, as when the proxy handles TLS, start the server with
--behind-proxy so it reads each player's address from the X-Forwarded-For
header the proxy adds, rather than seeing every player as the proxy.
"""

import argparse
import asyncio
import http
import json
import time

from websockets.asyncio.server import serve
from websockets.exceptions import ConnectionClosed

import core
from memory_store import MemoryStore

MAX_SESSIONS = 1000
MAX_MESSAGE_SIZE = 64 * 1024
MESSAGE_RATE = 50.0
MESSAGE_BURST = 200.0


class RateLimit:
    """A token bucket: `rate` tokens a second, holding at most `burst`."""

    def __init__(self, rate, burst, now=time.monotonic):
        self.rate = rate
        self.burst = burst
        self.now = now
        self.tokens = burst
        self.last = now()

    def allow(self):
        """Takes a token if there is one; says whether there was."""
        at = self.now()
        self.tokens = min(self.burst, self.tokens + (at - self.last) * self.rate)
        self.last = at
        if self.tokens < 1:
            return False
        self.tokens -= 1
        return True


class Address:
    """What one address is using here: its connections and message rate."""

    def __init__(self):
        self.connections = 0
        self.rate = RateLimit(MESSAGE_RATE, MESSAGE_BURST)


signaling = core.Signaling(MemoryStore(), max_sessions=MAX_SESSIONS)
sockets = {}  # connection name -> websocket
addresses = {}  # address -> Address
behind_proxy = False


def reset():
    """Forgets every session and connection, as a fresh start would."""
    global signaling
    signaling = core.Signaling(MemoryStore(), max_sessions=MAX_SESSIONS)
    sockets.clear()
    addresses.clear()


def address_of(ws):
    """The address a connection comes from: the one the proxy saw, behind a
    proxy (the last in X-Forwarded-For, which the proxy itself added)."""
    if behind_proxy:
        forwarded = ws.request.headers.get("X-Forwarded-For", "") if ws.request else ""
        last = forwarded.split(",")[-1].strip()
        if last:
            return last
    return ws.remote_address[0] if ws.remote_address else "?"


async def deliver(out):
    """Sends what the rules said to send, in order."""
    for connection, msg in out:
        ws = sockets.get(connection)
        if ws is None:
            continue
        try:
            if msg is None:
                await ws.close()
            else:
                await ws.send(json.dumps(msg))
        except ConnectionClosed:
            pass


def answer_http(ws, request):
    """Answers the plain HTTP addresses; leaves WebSocket requests alone."""
    if request.headers.get("Upgrade", "").lower() == "websocket":
        return None
    path = request.path.split("?")[0].rstrip("/")
    if path == "/sessions":
        body = {"sessions": signaling.list_sessions()}
    elif path.startswith("/sessions/"):
        body = {"open": signaling.session_open(path.removeprefix("/sessions/"))}
    else:
        return ws.respond(http.HTTPStatus.NOT_FOUND, "Not found.\n")
    response = ws.respond(http.HTTPStatus.OK, json.dumps(body))
    response.headers["Content-Type"] = "application/json"
    # The game's page comes from another address, and may read this from there.
    response.headers["Access-Control-Allow-Origin"] = "*"
    response.headers["Cache-Control"] = "no-store"
    return response


async def handle(ws):
    address = address_of(ws)
    connection = str(ws.id)
    if not signaling.connect(connection, address):
        await ws.close(1008, "Too many connections from this address.")
        return
    sockets[connection] = ws
    usage = addresses.setdefault(address, Address())
    usage.connections += 1
    try:
        async for raw in ws:
            if not usage.rate.allow():
                await deliver([(connection, {"type": "error", "message": "Too many messages. Slow down."})])
                continue
            await deliver(signaling.message(connection, raw))
    except ConnectionClosed:
        pass
    finally:
        out = signaling.disconnect(connection)
        del sockets[connection]
        usage.connections -= 1
        if usage.connections == 0:
            addresses.pop(address, None)
        await deliver(out)


def serving(host, port):
    """The server, with its limits, ready to start with `async with`."""
    return serve(handle, host, port, max_size=MAX_MESSAGE_SIZE, process_request=answer_http)


async def main():
    global behind_proxy
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--host", default="0.0.0.0")
    parser.add_argument("--port", type=int, default=9080)
    parser.add_argument("--behind-proxy", action="store_true", help="take players' addresses from X-Forwarded-For")
    args = parser.parse_args()
    behind_proxy = args.behind_proxy
    async with serving(args.host, args.port):
        print(f"signaling server listening on ws://{args.host}:{args.port}", flush=True)
        await asyncio.Future()


if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        pass
