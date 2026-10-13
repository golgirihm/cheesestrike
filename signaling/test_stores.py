"""Tests for the signaling rules on their own, with no network, run over each
place they can keep sessions: memory, as the development server does, and
DynamoDB, as on AWS. Both have to behave the same.

DynamoDB here is moto's imitation of it, in this process. Nothing reaches AWS.

    python -m unittest discover -s signaling
"""

import json
import os
import unittest

import boto3
from moto import mock_aws

import core
import dynamo_store
from memory_store import MemoryStore


class Clock:
    def __init__(self):
        self.time = 1_000_000.0

    def __call__(self):
        return self.time


class RulesTests:
    """The tests, for whichever store `make_store` gives. Connections are
    named by short strings; `self.said` collects what each has been sent."""

    def setUp(self):
        self.clock = Clock()
        self.signaling = core.Signaling(self.make_store(), now=self.clock)
        self.said = {}
        self.closed = set()

    def limit(self, name, value):
        was = getattr(core, name)
        setattr(core, name, value)
        self.addCleanup(setattr, core, name, was)

    def deliver(self, out):
        for connection, msg in out:
            if msg is None:
                self.closed.add(connection)
            else:
                self.said.setdefault(connection, []).append(msg)

    def connect(self, connection, address="10.0.0.1"):
        allowed = self.signaling.connect(connection, address)
        if not allowed:
            self.closed.add(connection)
        return allowed

    def send(self, connection, **msg):
        self.deliver(self.signaling.message(connection, json.dumps(msg)))

    def disconnect(self, connection):
        self.deliver(self.signaling.disconnect(connection))

    def last(self, connection):
        return self.said[connection][-1]

    def host(self, connection="host", address="10.0.0.1", **options):
        """Hosts a session from a new connection; returns (code, token)."""
        self.connect(connection, address)
        self.send(connection, type="host", **options)
        hosted = self.last(connection)
        self.assertEqual(hosted["type"], "hosted", hosted)
        return hosted["code"], hosted["token"]

    def join(self, code, connection):
        """Joins from a new connection; returns the peer id."""
        self.connect(connection)
        self.send(connection, type="join", code=code)
        joined = self.last(connection)
        self.assertEqual(joined["type"], "joined", joined)
        return joined["id"]

    def codes_listed(self):
        return [session["code"] for session in self.signaling.list_sessions()]

    def test_hosting_gives_a_code_and_a_token_and_lists_the_session(self):
        code, token = self.host(settings={"map": "city"})
        self.assertTrue(core.valid_code(code))
        self.assertGreaterEqual(len(token), 16)
        self.assertEqual(self.signaling.list_sessions(), [{"code": code, "players": 1, "settings": core.DEFAULT_SETTINGS | {"map": "city"}}])
        self.assertTrue(self.signaling.session_open(code))

    def test_a_private_session_is_open_but_not_listed(self):
        code, _ = self.host(private=True)
        self.assertEqual(self.signaling.list_sessions(), [])
        self.assertTrue(self.signaling.session_open(code))
        self.join(code, "joiner")

    def test_the_list_is_newest_first_and_only_so_long(self):
        self.limit("LIST_LIMIT", 3)
        codes = []
        for number in range(5):
            self.clock.time += 1
            codes.append(self.host(f"host{number}", address=f"10.0.0.{number}")[0])
        self.assertEqual(self.codes_listed(), codes[:1:-1])

    def test_two_sessions_are_never_given_the_same_code(self):
        # With a one-letter code, hosts collide at once; each still gets its own.
        self.limit("CODE_LENGTH", 1)
        self.limit("CODE_ATTEMPTS", 500)
        codes = [self.host(f"host{number}", address=f"10.0.0.{number}")[0] for number in range(len(core.CODE_ALPHABET))]
        self.assertEqual(sorted(codes), sorted(core.CODE_ALPHABET))

    def test_when_every_code_is_taken_hosting_is_refused(self):
        self.limit("CODE_LENGTH", 1)
        self.limit("CODE_ALPHABET", "AB")
        self.host("first", address="10.0.0.1")
        self.host("second", address="10.0.0.2")
        self.connect("third", "10.0.0.3")
        self.send("third", type="host")
        self.assertEqual(self.last("third"), {"type": "error", "message": "The session server is full."})

    def test_joining_tells_both_sides_and_messages_pass_between_them(self):
        code, _ = self.host()
        peer = self.join(code, "joiner")
        self.assertNotEqual(peer, core.HOST_ID)
        self.assertEqual(self.last("host"), {"type": "peer_joined", "id": peer})

        self.send("joiner", type="offer", sdp="the offer")
        self.assertEqual(self.last("host"), {"type": "offer", "sdp": "the offer", "from": peer})
        self.send("host", type="answer", to=peer, sdp="the answer")
        self.assertEqual(self.last("joiner"), {"type": "answer", "sdp": "the answer", "from": core.HOST_ID})

    def test_a_host_reaches_only_the_joiner_it_names(self):
        code, _ = self.host()
        first = self.join(code, "first")
        second = self.join(code, "second")
        self.send("host", type="candidate", to=second, sdp="c")
        self.assertEqual(self.last("second")["type"], "candidate")
        self.assertEqual(self.last("first")["type"], "joined", "nothing more was sent to the other joiner")
        self.send("host", type="candidate", to=12345, sdp="c")
        self.send("first", type="candidate", to=second, sdp="c")
        self.assertEqual(self.last("host")["from"], first, "a joiner's message goes to the host whatever it names")

    def test_someone_who_has_not_joined_cant_send_to_anyone(self):
        code, _ = self.host()
        self.connect("stranger")
        self.send("stranger", type="offer", to=core.HOST_ID, sdp="x")
        self.assertEqual(self.last("host")["type"], "hosted")

    def test_joining_an_unknown_or_malformed_code_is_refused(self):
        self.host()
        self.connect("joiner")
        for code in ("ZZZZ", "", "toolong", 7, None, "s#AB"):
            self.send("joiner", type="join", code=code)
            self.assertEqual(self.last("joiner"), {"type": "error", "message": "No session with that code."})

    def test_a_session_is_full_counting_the_hosts_number_and_those_still_joining(self):
        code, _ = self.host(settings={"max_players": 4})
        self.send("host", type="update", players=2)
        self.join(code, "first")
        self.join(code, "second")
        self.connect("third")
        self.send("third", type="join", code=code)
        self.assertEqual(self.last("third"), {"type": "error", "message": "That session is full."})

        # One connects to the host, which counts them, and leaves the server.
        self.send("host", type="update", players=3)
        self.disconnect("first")
        self.send("third", type="join", code=code)
        self.assertEqual(self.last("third"), {"type": "error", "message": "That session is full."})
        # The other gives up, which frees a place.
        self.disconnect("second")
        self.send("third", type="join", code=code)
        self.assertEqual(self.last("third")["type"], "joined")

    def test_the_hosts_count_is_kept_within_its_session(self):
        code, _ = self.host(settings={"max_players": 4})
        for given, listed in ((3, 3), (99, 4), (0, 1), (-5, 1), ("many", 1), (True, 1), (2.9, 2)):
            self.send("host", type="update", players=given)
            self.assertEqual(self.signaling.list_sessions()[0]["players"], listed, given)

    def test_only_the_host_can_change_the_count(self):
        code, _ = self.host()
        self.join(code, "joiner")
        self.send("joiner", type="update", players=9)
        self.connect("stranger")
        self.send("stranger", type="update", players=9)
        self.assertEqual(self.signaling.list_sessions()[0]["players"], 1)

    def test_a_joiner_leaving_is_reported_to_the_host(self):
        code, _ = self.host()
        peer = self.join(code, "joiner")
        self.disconnect("joiner")
        self.assertEqual(self.last("host"), {"type": "peer_left", "id": peer})
        # And can no longer be reached.
        self.send("host", type="answer", to=peer, sdp="late")
        self.assertEqual(self.last("joiner")["type"], "joined")

    def test_a_host_leaving_closes_the_session_and_those_still_joining(self):
        code, _ = self.host()
        self.join(code, "joiner")
        self.disconnect("host")
        self.assertEqual(self.last("joiner"), {"type": "closed"})
        self.assertIn("joiner", self.closed)
        self.assertEqual(self.signaling.list_sessions(), [])
        self.assertFalse(self.signaling.session_open(code))
        self.connect("late")
        self.send("late", type="join", code=code)
        self.assertEqual(self.last("late"), {"type": "error", "message": "No session with that code."})

    def test_a_host_takes_its_session_back_on_a_new_connection(self):
        code, token = self.host(settings={"map": "box"})
        self.send("host", type="update", players=3)
        self.disconnect("host")

        self.connect("host again", "10.9.9.9")
        self.send("host again", type="resume", code=code, token=token)
        self.assertEqual(self.last("host again"), {"type": "resumed", "code": code})
        self.assertEqual(self.signaling.list_sessions(), [{"code": code, "players": 3, "settings": core.DEFAULT_SETTINGS | {"map": "box"}}])
        self.assertTrue(self.signaling.session_open(code))
        peer = self.join(code, "joiner")
        self.assertEqual(self.last("host again"), {"type": "peer_joined", "id": peer})
        self.send("host again", type="update", players=4)
        self.assertEqual(self.signaling.list_sessions()[0]["players"], 4)

    def test_a_private_session_stays_unlisted_when_resumed(self):
        code, token = self.host(private=True)
        self.disconnect("host")
        self.connect("host again")
        self.send("host again", type="resume", code=code, token=token)
        self.assertEqual(self.signaling.list_sessions(), [])
        self.assertTrue(self.signaling.session_open(code))

    def test_the_old_connection_closing_late_doesnt_close_a_resumed_session(self):
        # The new connection can arrive before the server hears the old one end.
        code, token = self.host()
        self.connect("host again")
        self.send("host again", type="resume", code=code, token=token)
        self.assertEqual(self.last("host again")["type"], "resumed")
        self.disconnect("host")
        self.assertTrue(self.signaling.session_open(code))
        self.assertEqual(self.codes_listed(), [code])
        self.send("host", type="update", players=9)
        self.assertEqual(self.signaling.list_sessions()[0]["players"], 1)

    def test_resuming_needs_the_sessions_token(self):
        code, token = self.host()
        self.connect("thief")
        for attempt in ({"token": "a guess"}, {"token": ""}, {"token": None}, {}, {"token": token, "code": "nope"}):
            self.send("thief", **{"type": "resume", "code": code} | attempt)
            self.assertEqual(self.last("thief"), {"type": "error", "message": "That session can't be resumed."})
        self.join(code, "joiner")
        self.assertEqual(self.last("host")["type"], "peer_joined", "the session is still its host's")

    def test_a_session_whose_host_stays_away_is_forgotten_but_can_be_remade(self):
        code, token = self.host(settings={"map": "box"}, private=True)
        self.disconnect("host")
        self.clock.time += core.RESUME_GRACE + 1
        self.assertFalse(self.signaling.session_open(code))

        # Its players are still connected to it, so it asks for the same code.
        self.connect("host again")
        self.send("host again", type="resume", code=code, token=token, settings={"map": "box"}, private=False, players=5)
        self.assertEqual(self.last("host again"), {"type": "resumed", "code": code})
        self.assertEqual(self.signaling.list_sessions(), [{"code": code, "players": 5, "settings": core.DEFAULT_SETTINGS | {"map": "box"}}])

    def test_a_forgotten_sessions_code_cant_be_remade_once_someone_else_has_it(self):
        self.limit("CODE_LENGTH", 1)
        self.limit("CODE_ALPHABET", "A")
        code, token = self.host()
        self.disconnect("host")
        self.clock.time += core.RESUME_GRACE + 1
        self.host("another", address="10.0.0.2")
        self.connect("host again")
        self.send("host again", type="resume", code=code, token=token)
        self.assertEqual(self.last("host again"), {"type": "error", "message": "That session can't be resumed."})

    def test_a_session_not_heard_from_is_forgotten_and_a_ping_keeps_it(self):
        quiet, _ = self.host("quiet", address="10.0.0.1")
        pinging, _ = self.host("pinging", address="10.0.0.2")
        self.clock.time += core.SESSION_LIFE - 10
        self.send("pinging", type="ping")
        self.clock.time += 20
        self.assertEqual(self.codes_listed(), [pinging])
        self.assertFalse(self.signaling.session_open(quiet))
        self.assertTrue(self.signaling.session_open(pinging))

    def test_one_address_can_only_have_so_many_connections(self):
        self.limit("MAX_CONNECTIONS_PER_ADDRESS", 2)
        self.assertTrue(self.connect("a"))
        self.assertTrue(self.connect("b"))
        self.assertFalse(self.connect("c"))
        self.assertTrue(self.connect("elsewhere", "10.0.0.2"))
        self.disconnect("a")
        self.assertTrue(self.connect("c"))

    def test_a_connection_that_was_never_closed_stops_counting_in_time(self):
        self.limit("MAX_CONNECTIONS_PER_ADDRESS", 1)
        self.connect("lost")
        self.clock.time += core.CONNECTION_LIFE + 1
        self.assertTrue(self.connect("new"))

    def test_one_address_can_only_host_so_many_sessions(self):
        self.limit("MAX_SESSIONS_PER_ADDRESS", 2)
        self.host("first")
        self.host("second")
        self.connect("third")
        self.send("third", type="host")
        self.assertEqual(self.last("third"), {"type": "error", "message": "Too many sessions hosted from this address."})
        self.host("elsewhere", address="10.0.0.2")
        self.disconnect("first")
        self.send("third", type="host")
        self.assertEqual(self.last("third")["type"], "hosted")

    def test_a_connection_hosts_or_joins_only_once(self):
        code, _ = self.host()
        self.send("host", type="host")
        self.send("host", type="join", code=code)
        self.assertEqual(len(self.said["host"]), 1)
        self.assertEqual(self.codes_listed(), [code])

    def test_malformed_messages_are_refused(self):
        self.connect("someone")
        for raw in ("not json", "[]", "7", '{"no": "type"}', "null"):
            self.deliver(self.signaling.message("someone", raw))
            self.assertEqual(self.last("someone"), {"type": "error", "message": "Malformed message."})

    def test_an_unknown_connection_is_told_and_closed(self):
        self.send("nobody", type="ping")
        self.assertEqual(self.last("nobody")["type"], "error")
        self.assertIn("nobody", self.closed)
        self.assertEqual(self.signaling.disconnect("nobody"), [])


class MemoryRulesTest(RulesTests, unittest.TestCase):
    def make_store(self):
        return MemoryStore()

    def test_the_whole_server_can_be_limited_to_so_many_sessions(self):
        self.signaling.max_sessions = 2
        self.host("first", address="10.0.0.1")
        self.host("second", address="10.0.0.2")
        self.connect("third", "10.0.0.3")
        self.send("third", type="host")
        self.assertEqual(self.last("third"), {"type": "error", "message": "The session server is full."})


class DynamoRulesTest(RulesTests, unittest.TestCase):
    tables = 0

    @classmethod
    def setUpClass(cls):
        # Made-up credentials, so that nothing can reach a real account even
        # if the imitation were somehow bypassed.
        for name in ("AWS_ACCESS_KEY_ID", "AWS_SECRET_ACCESS_KEY", "AWS_SESSION_TOKEN"):
            os.environ[name] = "testing"
        os.environ["AWS_DEFAULT_REGION"] = "us-east-2"
        mock = mock_aws()
        mock.start()
        cls.addClassCleanup(mock.stop)
        cls.dynamodb = boto3.resource("dynamodb")

    def make_store(self):
        # A table of its own for each test, which is quicker than starting
        # the imitation afresh.
        DynamoRulesTest.tables += 1
        return dynamo_store.DynamoStore(dynamo_store.create_table(self.dynamodb, f"signaling-{self.tables}"))


if __name__ == "__main__":
    unittest.main()
