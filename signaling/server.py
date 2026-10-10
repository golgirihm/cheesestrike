"""Signaling server for CheeseStrike.

Lists open sessions and relays WebRTC handshake messages between a session's
host and the players joining it. No game traffic passes through here: once two
browsers have connected to each other they stop needing this server.

Protocol: JSON text frames over WebSocket.

  client -> server
    {"type": "list"}
    {"type": "host"}
    {"type": "join", "code": "ABCD"}
    {"type": "offer" | "answer" | "candidate", "to": <peer id>, ...}

  server -> client
    {"type": "sessions", "sessions": [{"code": "ABCD", "players": 3}]}
    {"type": "hosted", "code": "ABCD"}               to the host
    {"type": "joined", "code": "ABCD", "id": <id>}   to the joiner
    {"type": "peer_joined" | "peer_left", "id": <id>}  to the host
    {"type": "closed"}                               to joiners, when the host drops
    {"type": "offer" | "answer" | "candidate", "from": <peer id>, ...}
    {"type": "error", "message": "..."}

The host is always peer 1. Joiners only ever exchange messages with the host.
"""

import argparse
import asyncio
import json
import random

from websockets.asyncio.server import serve
from websockets.exceptions import ConnectionClosed

HOST_ID = 1
MAX_PLAYERS = 16
MAX_SESSIONS = 1000
CODE_ALPHABET = "ABCDEFGHJKLMNPQRSTUVWXYZ"  # no I or O
CODE_LENGTH = 4
RELAYED = ("offer", "answer", "candidate")


class Session:
    def __init__(self, code, host):
        self.code = code
        self.peers = {HOST_ID: host}  # peer id -> websocket


sessions = {}  # code -> Session


async def send(ws, **msg):
    try:
        await ws.send(json.dumps(msg))
    except ConnectionClosed:
        pass


def new_code():
    while True:
        code = "".join(random.choices(CODE_ALPHABET, k=CODE_LENGTH))
        if code not in sessions:
            return code


async def handle(ws):
    session = None
    peer_id = None
    try:
        async for raw in ws:
            try:
                msg = json.loads(raw)
                kind = msg["type"]
            except (ValueError, TypeError, KeyError):
                await send(ws, type="error", message="Malformed message.")
                continue

            if kind == "list":
                listing = [{"code": s.code, "players": len(s.peers)} for s in sessions.values()]
                await send(ws, type="sessions", sessions=listing)

            elif kind == "host" and session is None:
                if len(sessions) >= MAX_SESSIONS:
                    await send(ws, type="error", message="The session server is full.")
                    continue
                session = Session(new_code(), ws)
                peer_id = HOST_ID
                sessions[session.code] = session
                print(f"[{session.code}] hosted")
                await send(ws, type="hosted", code=session.code)

            elif kind == "join" and session is None:
                target = sessions.get(str(msg.get("code", "")).upper())
                if target is None:
                    await send(ws, type="error", message="No session with that code.")
                    continue
                if len(target.peers) >= MAX_PLAYERS:
                    await send(ws, type="error", message="That session is full.")
                    continue
                session = target
                peer_id = random.randrange(2, 2**31)
                while peer_id in session.peers:
                    peer_id = random.randrange(2, 2**31)
                session.peers[peer_id] = ws
                print(f"[{session.code}] peer {peer_id} joined ({len(session.peers)} players)")
                await send(ws, type="joined", code=session.code, id=peer_id)
                await send(session.peers[HOST_ID], type="peer_joined", id=peer_id)

            elif kind in RELAYED and session is not None:
                to = HOST_ID if peer_id != HOST_ID else msg.get("to")
                target_ws = session.peers.get(to)
                if target_ws is not None:
                    relayed = {k: v for k, v in msg.items() if k != "to"}
                    relayed["from"] = peer_id
                    await send(target_ws, **relayed)
    except ConnectionClosed:
        pass
    finally:
        if session is None:
            pass  # Never hosted or joined, so there is nothing to clean up.
        elif peer_id == HOST_ID:
            # WebRTC links between host and players are already up and survive
            # this; the session just stops being listed or joinable.
            sessions.pop(session.code, None)
            print(f"[{session.code}] closed")
            for other_id, other in list(session.peers.items()):
                if other_id != HOST_ID:
                    await send(other, type="closed")
                    await other.close()
        else:
            session.peers.pop(peer_id, None)
            print(f"[{session.code}] peer {peer_id} left")
            if session.code in sessions:
                await send(session.peers[HOST_ID], type="peer_left", id=peer_id)


async def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--host", default="0.0.0.0")
    parser.add_argument("--port", type=int, default=9080)
    args = parser.parse_args()
    async with serve(handle, args.host, args.port, max_size=64 * 1024):
        print(f"signaling server listening on ws://{args.host}:{args.port}", flush=True)
        await asyncio.Future()


if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        pass
