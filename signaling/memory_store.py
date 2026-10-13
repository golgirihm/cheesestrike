"""Where the signaling rules keep sessions and connections when one process
runs them: in memory. signaling/dynamo_store.py keeps the same things in
DynamoDB, with the same methods, and signaling/test_stores.py holds both to
the same behavior.

A session is a dict: code, token, host (its connection, or None while it is
away), address, settings, private, players, peers (peer id as a string ->
connection, for joiners still connecting), created, expires. A connection's
record is a dict: address, expires, and once it has hosted or joined, role
("host" or "joiner"), code and peer.

Anything past its `expires` time is treated as gone, whether or not it has
been cleared away yet.
"""

import copy


class MemoryStore:
    def __init__(self):
        self.sessions = {}  # code -> session
        self.connections = {}  # connection -> record

    def add_connection(self, connection, address, expires):
        self.connections[connection] = {"address": address, "expires": expires}

    def connection(self, connection, now):
        record = self.connections.get(connection)
        return dict(record) if record is not None and record["expires"] > now else None

    def set_connection(self, connection, role, code, peer):
        if connection in self.connections:
            self.connections[connection].update(role=role, code=code, peer=peer)

    def remove_connection(self, connection):
        return self.connections.pop(connection, None)

    def count_connections(self, address, now):
        return sum(1 for record in self.connections.values() if record["address"] == address and record["expires"] > now)

    def count_sessions(self, address, now):
        """How many sessions `address` is hosting now. One whose host is away
        doesn't count, or a host that kept reconnecting would run out."""
        return sum(1 for s in self._live(now) if s["address"] == address and s["host"] is not None)

    def count_all_sessions(self, now):
        return len(self._live(now))

    def create_session(self, session):
        """Keeps a new session, unless its code is taken. Says whether it did."""
        existing = self.sessions.get(session["code"])
        if existing is not None and existing["expires"] > session["created"]:
            return False
        self.sessions[session["code"]] = copy.deepcopy(session)
        return True

    def session(self, code, now):
        session = self.sessions.get(code)
        return copy.deepcopy(session) if session is not None and session["expires"] > now else None

    def update_session(self, code, host, fields):
        """Changes a session, but only for the connection that is its host."""
        session = self.sessions.get(code)
        if session is None or session["host"] != host:
            return False
        session.update(fields)
        return True

    def release_host(self, code, host, expires):
        """Marks a session's host as away, if `host` is still its host."""
        return self.update_session(code, host, {"host": None, "expires": expires})

    def claim_host(self, code, token, host, address, expires):
        """Makes `host` a session's host, for whoever holds its token."""
        session = self.sessions.get(code)
        if session is None or session["token"] != token:
            return False
        session.update(host=host, address=address, expires=expires)
        return True

    def set_peer(self, code, peer, connection):
        if code in self.sessions:
            self.sessions[code]["peers"][str(peer)] = connection

    def remove_peer(self, code, peer):
        if code in self.sessions:
            self.sessions[code]["peers"].pop(str(peer), None)

    def public_sessions(self, limit, now):
        """Up to `limit` open sessions that aren't private, newest first."""
        listed = [s for s in self._live(now) if not s["private"] and s["host"] is not None]
        listed.sort(key=lambda s: s["created"], reverse=True)
        return copy.deepcopy(listed[:limit])

    def _live(self, now):
        # Clearing out what has expired here keeps memory from growing.
        for code in [code for code, s in self.sessions.items() if s["expires"] <= now]:
            del self.sessions[code]
        return list(self.sessions.values())
