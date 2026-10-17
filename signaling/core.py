"""The signaling rules for CheeseStrike, apart from how messages travel and
where sessions are kept.

Signaling lists open sessions and relays WebRTC handshake messages between a
session's host and the players joining it. No game traffic passes through
here: once two browsers have connected to each other they stop needing it.

Two things run these rules. signaling/local/server.py is one process holding
everything in memory, for development and the tests. signaling/cloud/aws_lambda.py
runs them on AWS, where nothing stays running between messages and sessions
are kept in DynamoDB. docs/backend.md describes the whole arrangement.

Who connects, and for how long:

  - A player on the menu holds no connection. The session list is fetched
    over HTTP: `Signaling.list_sessions`.
  - A player joining opens a connection, exchanges the handshake with the
    host, and closes it once the two are connected to each other.
  - A host keeps one connection for as long as its session is open, since a
    player may ask to join at any time.
  - A player who stops hearing their host asks over HTTP whether the session
    is still open: `Signaling.session_open`.

Protocol: JSON text messages.

  host -> server
    {"type": "host", "settings": {...}, "private": true}   both optional; see below
    {"type": "resume", "code": "ABCD", "token": "...", "settings": {...}, "private": true, "players": 3}
    {"type": "update", "players": 3}
    {"type": "ping"}
  joiner -> server
    {"type": "join", "code": "ABCD"}
  either -> server
    {"type": "offer" | "answer" | "candidate", "to": <peer id>, ...}

  server -> client
    {"type": "hosted", "code": "ABCD", "token": "..."}  to the host
    {"type": "resumed", "code": "ABCD"}                 to the host
    {"type": "joined", "code": "ABCD", "id": <id>}      to the joiner
    {"type": "peer_joined" | "peer_left", "id": <id>}   to the host
    {"type": "closed"}                                  to joiners still connecting, when the host drops
    {"type": "offer" | "answer" | "candidate", "from": <peer id>, ...}
    {"type": "error", "message": "..."}

The host is always peer 1. Joiners only ever exchange messages with the host.

A host's settings are how its matches are set up, which players see in the list
before joining: {"mode": "ffa" | "teams", "kill_limit": n, "minutes": n,
"max_players": n, "map": "canyon" | "box" | "city"}. The server only checks them and passes them on, apart
from max_players, which it enforces. Anything missing or out of range is replaced by
the default, as the game itself does.

The server can't see who is playing, since players leave it once they are
connected to their host. The host says how many there are with `update`, and
that is the number listed.

A host that loses its connection, as every connection does sooner or later,
connects again and takes its session back with `resume` and the token it was
given when it hosted. Until then the session is not open: it is left out of
the list and can't be joined. A session whose host hasn't come back within
RESUME_GRACE seconds is forgotten; a host that returns later still gets its
code back if nobody else has it. `ping` keeps a quiet connection from being
closed as idle, and tells the server the session is still wanted: one that
has not been heard from for SESSION_LIFE seconds is forgotten too.

Limits, so that one machine can't flood the server: each address may have
MAX_CONNECTIONS_PER_ADDRESS connections open and MAX_SESSIONS_PER_ADDRESS
sessions hosted at once. Limits on message size and rate belong to whatever
carries the messages.

Names never pass through here: players exchange them with the host directly,
and the game cleans them (game/match/player_name.gd).

A private session is left out of the list; players join it by its code, given
to them by the host.
"""

import json
import math
import random
import secrets
import time

HOST_ID = 1
MAX_PLAYERS = 16
CODE_ALPHABET = "ABCDEFGHJKLMNPQRSTUVWXYZ"  # no I or O
CODE_LENGTH = 4
RELAYED = ("offer", "answer", "candidate")
MODES = ("ffa", "teams")
MAPS = ("canyon", "box", "city")
DEFAULT_SETTINGS = {"mode": "ffa", "kill_limit": 20, "minutes": 8, "max_players": MAX_PLAYERS, "map": "canyon"}
MAX_CONNECTIONS_PER_ADDRESS = 32
MAX_SESSIONS_PER_ADDRESS = 8
# Allowed range of each number in a host's settings.
SETTING_RANGES = {"kill_limit": (1, 999), "minutes": (1, 60), "max_players": (2, MAX_PLAYERS)}
# How many sessions the list holds. A player can't read more, and can join
# any session by its code whether or not it is listed.
LIST_LIMIT = 50
# How long a session waits for a host that lost its connection, in seconds.
RESUME_GRACE = 60
# How long a session lasts without hearing from its host, in seconds. Hosts
# ping far more often than this; it only ends sessions whose host vanished
# without the server being told.
SESSION_LIFE = 30 * 60
# How long a connection may last, in seconds, before it is forgotten: longer
# than anything carrying the messages allows one to stay open.
CONNECTION_LIFE = 3 * 60 * 60
# How many codes to try before giving up, when most are taken.
CODE_ATTEMPTS = 20


def clean_settings(given):
    """A host's settings with anything missing, of the wrong kind or out of
    range replaced by the default."""
    settings = dict(DEFAULT_SETTINGS)
    if not isinstance(given, dict):
        return settings
    if given.get("mode") in MODES:
        settings["mode"] = given["mode"]
    if given.get("map") in MAPS:
        settings["map"] = given["map"]
    for key, (low, high) in SETTING_RANGES.items():
        value = given.get(key)
        if _is_number(value):
            settings[key] = max(low, min(high, int(value)))
    return settings


def _is_number(value):
    # bool is an int to Python, but not a number of anything here.
    return isinstance(value, (int, float)) and not isinstance(value, bool) and math.isfinite(value)


def clean_players(given, settings):
    """How many players a host says it has, kept to what its session allows."""
    if not _is_number(given):
        return 1
    return max(1, min(settings["max_players"], int(given)))


def valid_code(code):
    return isinstance(code, str) and len(code) == CODE_LENGTH and all(letter in CODE_ALPHABET for letter in code)


class Signaling:
    """The rules, over a store that remembers sessions and connections.

    Each of `connect`, `message` and `disconnect` returns what to send as a
    result: a list of (connection, message) pairs, in order, where a message
    of None means close that connection. The caller does the sending, so the
    rules never wait on the network.

    A store is signaling/local/memory_store.py's MemoryStore or anything with the
    same methods; a connection is whatever the caller uses to name one.
    """

    def __init__(self, store, now=time.time, max_sessions=None):
        self.store = store
        self.now = now
        # None for no limit of our own, where the store has room for any number.
        self.max_sessions = max_sessions

    def connect(self, connection, address):
        """Notes a new connection. False if its address has too many open,
        in which case the caller should refuse it."""
        now = self.now()
        if self.store.count_connections(address, now) >= MAX_CONNECTIONS_PER_ADDRESS:
            return False
        self.store.add_connection(connection, address, now + CONNECTION_LIFE)
        return True

    def message(self, connection, raw):
        try:
            msg = json.loads(raw)
            kind = msg["type"]
        except (ValueError, TypeError, KeyError):
            return [(connection, _error("Malformed message."))]

        now = self.now()
        me = self.store.connection(connection, now)
        if me is None:
            return [(connection, _error("Not connected.")), (connection, None)]
        role = me.get("role")

        if kind == "host" and role is None:
            return self._host(connection, me, msg, now)
        if kind == "resume" and role is None:
            return self._resume(connection, me, msg, now)
        if kind == "join" and role is None:
            return self._join(connection, msg, now)
        if kind == "update" and role == "host":
            session = self.store.session(me["code"], now)
            if session is not None:
                players = clean_players(msg.get("players"), session["settings"])
                self.store.update_session(me["code"], connection, {"players": players, "expires": now + SESSION_LIFE})
            return []
        if kind == "ping":
            if role == "host":
                self.store.update_session(me["code"], connection, {"expires": now + SESSION_LIFE})
            return []
        if kind in RELAYED and role is not None:
            return self._relay(me, msg, now)
        return []

    def disconnect(self, connection):
        now = self.now()
        me = self.store.remove_connection(connection)
        if me is None or me.get("role") is None:
            return []  # Never hosted or joined, so there is nothing to clean up.
        code = me["code"]
        if me["role"] == "host":
            # WebRTC links between host and players are already up and survive
            # this; the session just stops being listed or joinable until the
            # host is back.
            session = self.store.session(code, now)
            if not self.store.release_host(code, connection, now + RESUME_GRACE):
                return []  # The host already took the session back on a new connection.
            out = []
            for joiner in (session or {}).get("peers", {}).values():
                out += [(joiner, {"type": "closed"}), (joiner, None)]
            return out
        self.store.remove_peer(code, me["peer"])
        session = self.store.session(code, now)
        if session is None or session.get("host") is None:
            return []
        # A joiner that is connected to the host leaves this way too; the host
        # only needs telling about one that gave up part-way.
        return [(session["host"], {"type": "peer_left", "id": me["peer"]})]

    def list_sessions(self):
        """The open, public sessions, as the menu shows them."""
        listed = self.store.public_sessions(LIST_LIMIT, self.now())
        return [{"code": s["code"], "players": s["players"], "settings": s["settings"]} for s in listed]

    def session_open(self, code):
        """Whether a session is open: it exists and its host is connected."""
        code = str(code).upper()
        if not valid_code(code):
            return False
        session = self.store.session(code, self.now())
        return session is not None and session.get("host") is not None

    def _host(self, connection, me, msg, now):
        if self.max_sessions is not None and self.store.count_all_sessions(now) >= self.max_sessions:
            return [(connection, _error("The session server is full."))]
        if self.store.count_sessions(me["address"], now) >= MAX_SESSIONS_PER_ADDRESS:
            return [(connection, _error("Too many sessions hosted from this address."))]
        session = {
            "token": secrets.token_urlsafe(16),
            "host": connection,
            "address": me["address"],
            "settings": clean_settings(msg.get("settings")),
            "private": msg.get("private") is True,
            "players": 1,
            "peers": {},
            "created": now,
            "expires": now + SESSION_LIFE,
        }
        for _ in range(CODE_ATTEMPTS):
            session["code"] = "".join(random.choices(CODE_ALPHABET, k=CODE_LENGTH))
            # Two hosts can pick the same code at the same moment; only one of
            # them is given it.
            if self.store.create_session(session):
                self.store.set_connection(connection, "host", session["code"], HOST_ID)
                return [(connection, {"type": "hosted", "code": session["code"], "token": session["token"]})]
        return [(connection, _error("The session server is full."))]

    def _resume(self, connection, me, msg, now):
        code, token = msg.get("code"), msg.get("token")
        refused = [(connection, _error("That session can't be resumed."))]
        if not valid_code(code) or not isinstance(token, str) or not token:
            return refused
        session = self.store.session(code, now)
        if session is None:
            # Forgotten while the host was away. Its players are still
            # connected to it, so give it the same code back if it is free.
            settings = clean_settings(msg.get("settings"))
            session = {
                "code": code,
                "token": token,
                "host": connection,
                "address": me["address"],
                "settings": settings,
                "private": msg.get("private") is True,
                "players": clean_players(msg.get("players"), settings),
                "peers": {},
                "created": now,
                "expires": now + SESSION_LIFE,
            }
            if not self.store.create_session(session):
                return refused
        elif not secrets.compare_digest(session["token"], token) or not self.store.claim_host(code, token, connection, me["address"], now + SESSION_LIFE):
            return refused
        self.store.set_connection(connection, "host", code, HOST_ID)
        return [(connection, {"type": "resumed", "code": code})]

    def _join(self, connection, msg, now):
        code = str(msg.get("code", "")).upper()
        session = self.store.session(code, now) if valid_code(code) else None
        if session is None or session.get("host") is None:
            return [(connection, _error("No session with that code."))]
        # The host's count, and those still connecting, whom it hasn't counted yet.
        if session["players"] + len(session["peers"]) >= session["settings"]["max_players"]:
            return [(connection, _error("That session is full."))]
        peer = random.randrange(2, 2**31)
        while str(peer) in session["peers"]:
            peer = random.randrange(2, 2**31)
        self.store.set_peer(code, peer, connection)
        self.store.set_connection(connection, "joiner", code, peer)
        return [
            (connection, {"type": "joined", "code": code, "id": peer}),
            (session["host"], {"type": "peer_joined", "id": peer}),
        ]

    def _relay(self, me, msg, now):
        session = self.store.session(me["code"], now)
        if session is None:
            return []
        if me["role"] == "host":
            target = session["peers"].get(str(msg.get("to")))
        else:
            target = session.get("host")
        if target is None:
            return []
        relayed = {key: value for key, value in msg.items() if key != "to"}
        relayed["from"] = me["peer"]
        return [(target, relayed)]


def _error(message):
    return {"type": "error", "message": message}
