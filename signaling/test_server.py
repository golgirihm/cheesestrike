"""Tests for the signaling server, run against a real instance on a spare port.

    python -m unittest discover -s signaling
"""

import asyncio
import json
import unittest

from websockets.asyncio.client import connect
from websockets.exceptions import ConnectionClosed

import server

TIMEOUT = 2


class ServerTestCase(unittest.IsolatedAsyncioTestCase):
    """Starts a server with no sessions for each test, and talks to it."""

    async def asyncSetUp(self):
        server.sessions.clear()
        server.addresses.clear()
        self.server = await server.serving("127.0.0.1", 0)
        port = self.server.sockets[0].getsockname()[1]
        self.url = f"ws://127.0.0.1:{port}"
        self.clients = []

    async def asyncTearDown(self):
        for ws in self.clients:
            await ws.close()
        self.server.close()
        await self.server.wait_closed()

    async def client(self, **options):
        ws = await connect(self.url, **options)
        self.clients.append(ws)
        return ws

    def limit(self, name, value):
        """Sets one of the server's limits for this test only."""
        was = getattr(server, name)
        setattr(server, name, value)
        self.addCleanup(setattr, server, name, was)

    async def closed_with(self, ws):
        """Waits for the server to close `ws`; returns the close code."""
        with self.assertRaises(ConnectionClosed):
            await asyncio.wait_for(ws.recv(), TIMEOUT)
        return ws.close_code

    async def send(self, ws, **msg):
        await ws.send(json.dumps(msg))

    async def recv(self, ws):
        return json.loads(await asyncio.wait_for(ws.recv(), TIMEOUT))

    async def host(self):
        ws = await self.client()
        await self.send(ws, type="host")
        return ws, (await self.recv(ws))["code"]

    async def join(self, code, host_ws):
        """Joins and returns (socket, peer id), consuming the host's notification."""
        ws = await self.client()
        await self.send(ws, type="join", code=code)
        joined = await self.recv(ws)
        self.assertEqual(joined["type"], "joined")
        await self.recv(host_ws)
        return ws, joined["id"]

    async def list_sessions(self):
        ws = await self.client()
        await self.send(ws, type="list")
        return (await self.recv(ws))["sessions"]


class SignalingTest(ServerTestCase):
    async def test_hosting_returns_a_code_and_lists_the_session(self):
        ws = await self.client()
        await self.send(ws, type="host")
        hosted = await self.recv(ws)
        self.assertEqual(hosted["type"], "hosted")
        self.assertEqual(await self.list_sessions(), [{"code": hosted["code"], "players": 1, "settings": server.DEFAULT_SETTINGS}])

    async def test_a_hosts_settings_are_listed(self):
        ws = await self.client()
        settings = {"mode": "teams", "kill_limit": 30, "minutes": 5, "max_players": 6, "map": "box"}
        await self.send(ws, type="host", settings=settings)
        await self.recv(ws)
        self.assertEqual((await self.list_sessions())[0]["settings"], settings)

    async def test_every_arena_can_be_hosted(self):
        for arena in server.MAPS:
            ws = await self.client()
            await self.send(ws, type="host", settings={"map": arena})
            hosted = await self.recv(ws)
            listed = next(session for session in await self.list_sessions() if session["code"] == hosted["code"])
            self.assertEqual(listed["settings"]["map"], arena)
        self.assertIn("city", server.MAPS)

    async def test_settings_out_of_range_are_brought_into_it(self):
        ws = await self.client()
        await self.send(ws, type="host", settings={"mode": "teams", "kill_limit": 0, "minutes": 1e9, "max_players": 99})
        await self.recv(ws)
        self.assertEqual(
            (await self.list_sessions())[0]["settings"],
            {"mode": "teams", "kill_limit": 1, "minutes": 60, "max_players": server.MAX_PLAYERS, "map": "canyon"},
        )

    async def test_settings_of_the_wrong_kind_are_replaced_by_the_defaults(self):
        ws = await self.client()
        await self.send(ws, type="host", settings={"mode": "hide and seek", "kill_limit": "lots", "minutes": True, "max_players": None, "map": "moon", "extra": 1})
        await self.recv(ws)
        self.assertEqual((await self.list_sessions())[0]["settings"], server.DEFAULT_SETTINGS)

    async def test_settings_that_are_not_finite_numbers_give_the_defaults(self):
        ws = await self.client()
        # JSON as Python reads it allows these, though the standard doesn't.
        await ws.send('{"type": "host", "settings": {"kill_limit": Infinity, "minutes": NaN, "max_players": -Infinity}}')
        await self.recv(ws)
        self.assertEqual((await self.list_sessions())[0]["settings"], server.DEFAULT_SETTINGS)

    async def test_settings_that_are_not_an_object_give_the_defaults(self):
        ws = await self.client()
        await self.send(ws, type="host", settings=["ffa", 20])
        await self.recv(ws)
        self.assertEqual((await self.list_sessions())[0]["settings"], server.DEFAULT_SETTINGS)

    async def test_a_private_session_is_not_listed_but_can_be_joined_by_code(self):
        public_ws, public_code = await self.host()
        private_ws = await self.client()
        await self.send(private_ws, type="host", private=True)
        private_code = (await self.recv(private_ws))["code"]
        self.assertEqual([session["code"] for session in await self.list_sessions()], [public_code])
        await self.join(private_code, private_ws)

    async def test_only_true_makes_a_session_private(self):
        for given in ("yes", 1, None, [True]):
            ws = await self.client()
            await self.send(ws, type="host", private=given)
            await self.recv(ws)
        self.assertEqual(len(await self.list_sessions()), 4)

    async def test_a_session_is_full_at_its_own_player_limit(self):
        host_ws = await self.client()
        await self.send(host_ws, type="host", settings={"max_players": 3})
        code = (await self.recv(host_ws))["code"]
        for _ in range(2):
            await self.join(code, host_ws)
        ws = await self.client()
        await self.send(ws, type="join", code=code)
        self.assertEqual(await self.recv(ws), {"type": "error", "message": "That session is full."})

    async def test_joining_tells_both_sides_the_new_peer_id(self):
        host_ws, code = await self.host()
        ws = await self.client()
        await self.send(ws, type="join", code=code)
        joined = await self.recv(ws)
        self.assertEqual(joined["type"], "joined")
        self.assertEqual(joined["code"], code)
        self.assertNotEqual(joined["id"], server.HOST_ID)
        self.assertEqual(await self.recv(host_ws), {"type": "peer_joined", "id": joined["id"]})
        self.assertEqual((await self.list_sessions())[0]["players"], 2)

    async def test_join_code_is_case_insensitive(self):
        host_ws, code = await self.host()
        ws = await self.client()
        await self.send(ws, type="join", code=code.lower())
        self.assertEqual((await self.recv(ws))["type"], "joined")

    async def test_joining_an_unknown_code_is_an_error(self):
        ws = await self.client()
        await self.send(ws, type="join", code="NOPE")
        self.assertEqual((await self.recv(ws))["type"], "error")

    async def test_joining_a_full_session_is_an_error(self):
        host_ws, code = await self.host()
        for _ in range(server.MAX_PLAYERS - 1):
            await self.join(code, host_ws)
        ws = await self.client()
        await self.send(ws, type="join", code=code)
        self.assertEqual((await self.recv(ws))["type"], "error")

    async def test_malformed_messages_get_an_error_and_keep_the_connection(self):
        ws = await self.client()
        await ws.send("not json")
        self.assertEqual((await self.recv(ws))["type"], "error")
        await self.send(ws, type="list")
        self.assertEqual((await self.recv(ws))["type"], "sessions")

    async def test_joiner_messages_are_relayed_to_the_host_with_the_sender(self):
        host_ws, code = await self.host()
        ws, peer_id = await self.join(code, host_ws)
        await self.send(ws, type="offer", to=server.HOST_ID, sdp="the offer")
        self.assertEqual(await self.recv(host_ws), {"type": "offer", "sdp": "the offer", "from": peer_id})

    async def test_host_messages_are_relayed_to_the_addressed_joiner_only(self):
        host_ws, code = await self.host()
        first, first_id = await self.join(code, host_ws)
        second, second_id = await self.join(code, host_ws)
        await self.send(host_ws, type="answer", to=second_id, sdp="the answer")
        self.assertEqual(await self.recv(second), {"type": "answer", "sdp": "the answer", "from": server.HOST_ID})
        with self.assertRaises(asyncio.TimeoutError):
            await asyncio.wait_for(first.recv(), 0.2)

    async def test_joiners_can_only_reach_the_host(self):
        host_ws, code = await self.host()
        first, first_id = await self.join(code, host_ws)
        second, second_id = await self.join(code, host_ws)
        await self.send(first, type="candidate", to=second_id, sdp="c")
        self.assertEqual((await self.recv(host_ws))["from"], first_id)
        with self.assertRaises(asyncio.TimeoutError):
            await asyncio.wait_for(second.recv(), 0.2)

    async def test_host_dropping_tells_joiners_and_closes_the_session(self):
        host_ws, code = await self.host()
        first, _ = await self.join(code, host_ws)
        second, _ = await self.join(code, host_ws)
        await host_ws.close()
        for ws in (first, second):
            self.assertEqual(await self.recv(ws), {"type": "closed"})
            with self.assertRaises(ConnectionClosed):
                await asyncio.wait_for(ws.recv(), TIMEOUT)
        self.assertEqual(await self.list_sessions(), [])

    async def test_joiner_dropping_tells_the_host_and_frees_the_slot(self):
        host_ws, code = await self.host()
        ws, peer_id = await self.join(code, host_ws)
        await ws.close()
        self.assertEqual(await self.recv(host_ws), {"type": "peer_left", "id": peer_id})
        self.assertEqual((await self.list_sessions())[0]["players"], 1)


class LimitsTest(ServerTestCase):
    async def test_a_message_too_large_closes_the_connection(self):
        ws = await self.client(max_size=None)
        await self.send(ws, type="list", padding="x" * server.MAX_MESSAGE_SIZE)
        self.assertEqual(await self.closed_with(ws), 1009)

    async def test_a_large_offer_within_the_limit_is_relayed(self):
        host_ws, code = await self.host()
        ws, peer_id = await self.join(code, host_ws)
        sdp = "a" * 20_000
        await self.send(ws, type="offer", sdp=sdp)
        self.assertEqual((await self.recv(host_ws))["sdp"], sdp)

    async def test_one_address_can_only_have_so_many_connections(self):
        self.limit("MAX_CONNECTIONS_PER_ADDRESS", 3)
        for _ in range(3):
            await self.list_sessions()
        extra = await self.client()
        self.assertEqual(await self.closed_with(extra), 1008)
        self.assertIn("Too many connections", extra.close_reason)

    async def test_closing_a_connection_frees_its_place(self):
        self.limit("MAX_CONNECTIONS_PER_ADDRESS", 1)
        first = await self.client()
        await first.close()
        await asyncio.sleep(0.1)
        self.assertEqual(await self.list_sessions(), [])

    async def test_one_address_can_only_host_so_many_sessions(self):
        self.limit("MAX_SESSIONS_PER_ADDRESS", 2)
        host_ws, _ = await self.host()
        await self.host()
        ws = await self.client()
        await self.send(ws, type="host")
        self.assertEqual(await self.recv(ws), {"type": "error", "message": "Too many sessions hosted from this address."})

        await host_ws.close()
        await asyncio.sleep(0.1)
        await self.send(ws, type="host")
        self.assertEqual((await self.recv(ws))["type"], "hosted")

    async def test_messages_beyond_the_rate_are_dropped_with_an_error(self):
        self.limit("MESSAGE_RATE", 0.0)
        self.limit("MESSAGE_BURST", 5.0)
        ws = await self.client()
        for _ in range(5):
            await self.send(ws, type="list")
            self.assertEqual((await self.recv(ws))["type"], "sessions")
        await self.send(ws, type="list")
        self.assertEqual(await self.recv(ws), {"type": "error", "message": "Too many messages. Slow down."})
        # Still connected, just held back.
        await self.send(ws, type="list")
        self.assertEqual((await self.recv(ws))["type"], "error")

    async def test_the_rate_is_shared_by_one_addresss_connections(self):
        self.limit("MESSAGE_RATE", 0.0)
        self.limit("MESSAGE_BURST", 2.0)
        await self.list_sessions()
        await self.list_sessions()
        ws = await self.client()
        await self.send(ws, type="list")
        self.assertEqual((await self.recv(ws))["type"], "error")

    async def test_behind_a_proxy_players_are_told_apart_by_the_forwarded_address(self):
        self.limit("behind_proxy", True)
        self.limit("MAX_CONNECTIONS_PER_ADDRESS", 1)
        await self.client(additional_headers={"X-Forwarded-For": "203.0.113.1"})
        other = await self.client(additional_headers={"X-Forwarded-For": "198.51.100.7, 203.0.113.2"})
        await self.send(other, type="list")
        self.assertEqual((await self.recv(other))["type"], "sessions")
        again = await self.client(additional_headers={"X-Forwarded-For": "203.0.113.1"})
        self.assertEqual(await self.closed_with(again), 1008)

    async def test_without_a_proxy_the_forwarded_address_is_ignored(self):
        self.limit("MAX_CONNECTIONS_PER_ADDRESS", 1)
        await self.client(additional_headers={"X-Forwarded-For": "203.0.113.1"})
        second = await self.client(additional_headers={"X-Forwarded-For": "203.0.113.2"})
        self.assertEqual(await self.closed_with(second), 1008)


class RateLimitTest(unittest.TestCase):
    def test_a_burst_then_the_steady_rate(self):
        clock = [0.0]
        limit = server.RateLimit(rate=2.0, burst=3.0, now=lambda: clock[0])
        self.assertEqual([limit.allow() for _ in range(4)], [True, True, True, False])
        clock[0] = 0.5
        self.assertEqual([limit.allow() for _ in range(2)], [True, False])
        clock[0] = 100.0
        self.assertEqual(sum(limit.allow() for _ in range(10)), 3, "never more than a burst saved up")


if __name__ == "__main__":
    unittest.main()
