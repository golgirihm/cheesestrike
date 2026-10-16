"""Hosting, finding, joining and losing sessions, through the real menu and a
real WebRTC connection between browser windows."""

import math
import re
import unittest

from harness import GameTestCase

NO_SESSIONS = "None yet. Host one!"
UNREACHABLE = "Can't reach the session server, so there are no sessions to show."


def distance(a, b):
    return math.dist(a, b)


class HostingTest(GameTestCase):
    def test_hosting_puts_you_in_a_session_of_one(self):
        game = self.open_game()
        code = game.host()
        self.assertRegex(code, r"^[A-Z]{4}$")
        state = game.wait_for("s.players.length === 1 && s.hud.includes('1 playing')")
        self.assertEqual(state["hud"], f"Session {code}  ·  1 playing")
        self.assertTrue(state["players"][0]["mine"])

    def test_hosting_without_the_session_server_fails_then_works_once_it_is_back(self):
        game = self.open_game()
        self.signaling.stop()
        game.click("HostButton")
        state = game.wait_for("s.status.startsWith(\"Can't reach\")")
        self.assertEqual(state["screen"], "menu")
        self.assertIn(f"ws://localhost:{self.signaling.port}", state["status"])
        self.assertIn("Hosting and joining need it", state["status"], "what it is for")
        self.assertIn("python signaling/server.py", state["status"], "and, in a development build, how to start it")
        # The list says why there is nothing in it, rather than that nobody is playing.
        state = game.wait_for(f"s.sessions.length === 1 && s.sessions[0].text === {UNREACHABLE!r}")

        self.signaling.start()
        game.click("RefreshButton")
        game.wait_for(f"s.sessions.length === 1 && s.sessions[0].text === {NO_SESSIONS!r}")
        game.host()


class JoiningTest(GameTestCase):
    def test_joining_by_a_code_typed_in_lower_case(self):
        host = self.open_game()
        code = host.host()
        joiner = self.open_game()
        joiner.join(code.lower())
        for game in (host, joiner):
            state = game.wait_for("s.players.length === 2 && s.hud.includes('2 playing')")
            self.assertEqual(state["code"], code)
            self.assertEqual(sum(player["mine"] for player in state["players"]), 1)

    def test_joining_from_the_session_list(self):
        joiner = self.open_game()
        state = joiner.wait_for("s.sessions.length === 1")
        self.assertEqual(state["sessions"][0]["text"], NO_SESSIONS)

        host = self.open_game()
        code = host.host()
        joiner.click("RefreshButton")
        listed = f"{code}  ·  1/16  ·  Free-for-all  ·  Cheddar Canyon"
        state = joiner.wait_for(f"s.sessions.length === 1 && s.sessions[0].text === '{listed}'")
        joiner.click_at(state["sessions"][0]["at"])
        for game in (host, joiner):
            game.wait_for("s.screen === 'game' && s.players.length === 2 && s.hud.includes('2 playing')")

    def test_play_now_joins_a_session_with_room(self):
        host = self.open_game()
        code = host.host()
        joiner = self.open_game()
        joiner.wait_for(f"s.sessions.length === 1 && s.sessions[0].text.startsWith('{code}')")
        joiner.click("PlayNowButton")
        for game in (host, joiner):
            state = game.wait_for("s.screen === 'game' && s.players.length === 2")
            self.assertEqual(state["code"], code)

    def test_play_now_with_nothing_listed_hosts(self):
        game = self.open_game()
        game.wait_for(f"s.sessions.length === 1 && s.sessions[0].text === '{NO_SESSIONS}'")
        game.click("PlayNowButton")
        state = game.wait_for("s.screen === 'game' && s.players.length === 1")
        self.assertRegex(state["code"], r"^[A-Z]{4}$")

    def test_the_list_can_be_filtered_by_arena(self):
        host = self.open_game()
        code = host.host()
        other = self.open_game()
        other.wait_for(f"s.sessions.length === 1 && s.sessions[0].text.startsWith('{code}')")
        # The first arena after "any" is Cheddar Canyon, where the host is;
        # the next is not.
        other.click("ArenaFilterButton")
        other.wait_for(f"s.sessions.length === 1 && s.sessions[0].text.startsWith('{code}')")
        other.click("ArenaFilterButton")
        other.wait_for("s.sessions.length === 1 && s.sessions[0].text === 'None like that. Host one!'")

    def test_the_list_shows_how_many_the_host_says_are_playing(self):
        # The session server can't count players, who leave it once they are
        # connected to their host, so the number is the host's.
        host = self.open_game()
        code = host.host()
        self.open_game().join(code)
        host.wait_for("s.players.length === 2")
        watcher = self.open_game()
        listed = f"{code}  ·  2/16  ·  Free-for-all  ·  Cheddar Canyon"
        for attempt in range(20):
            watcher.click("RefreshButton")
            try:
                watcher.wait_for(f"s.sessions.length === 1 && s.sessions[0].text === '{listed}'", timeout=1_000)
                return
            except AssertionError:
                continue
        self.fail(f"the list never showed two playing: {watcher.state()['sessions']}")

    def test_the_list_catches_up_without_being_asked(self):
        watcher = self.open_game()
        state = watcher.wait_for("s.sessions.length === 1")
        self.assertEqual(state["sessions"][0]["text"], NO_SESSIONS)
        code = self.open_game().host()
        # Nobody clicks Refresh: the menu asks again by itself every so often.
        watcher.wait_for(f"s.sessions.length === 1 && s.sessions[0].text.startsWith('{code}')")

    def test_a_host_takes_its_session_back_when_the_session_server_returns(self):
        # Every connection to the session server drops in the end. Here the
        # whole server goes, and comes back remembering nothing.
        host = self.open_game()
        code = host.host()
        self.signaling.stop()
        self.signaling.start()
        joiner = self.open_game()
        joiner.wait_for(f"s.sessions.length === 1 && s.sessions[0].text.startsWith('{code}')")
        joiner.join(code)
        for game in (host, joiner):
            state = game.wait_for("s.players.length === 2 && s.hud.includes('2 playing')")
            self.assertEqual(state["code"], code)

    def test_a_closed_session_leaves_the_list(self):
        host = self.open_game()
        code = host.host()
        other = self.open_game()
        other.wait_for(f"s.sessions.length === 1 && s.sessions[0].text.startsWith('{code}')")
        host.close()
        # The list only updates on request, and a request can reach the server
        # before it has noticed the host leave, so keep asking.
        gone = f"s.sessions.length === 1 && s.sessions[0].text === '{NO_SESSIONS}'"
        for attempt in range(20):
            other.click("RefreshButton")
            try:
                other.wait_for(gone, timeout=1_000)
                break
            except AssertionError:
                continue
        else:
            self.fail("the closed session stayed in the list")

    def test_an_empty_code_is_refused(self):
        game = self.open_game()
        game.click("JoinButton")
        state = game.wait_for("s.status === 'Enter a session code.'")
        self.assertEqual(state["screen"], "menu")

    def test_an_unknown_code_is_refused_and_the_menu_works_again(self):
        game = self.open_game()
        game.click("TargetEdit")
        game.type("ZZZZ")
        game.click("JoinButton")
        state = game.wait_for("s.status === 'No session with that code.'")
        self.assertEqual(state["screen"], "menu")
        game.host()

    def test_a_late_joiner_sees_everyone_where_they_now_are(self):
        host = self.open_game()
        code = host.host()
        second = self.open_game()
        second.join(code)

        # Walk both away from their starting spots, then let them come to rest.
        moved = {}
        for game in (host, second):
            start = game.own_player()["position"]
            # Until they have gone a little way, however long the machine
            # takes over it, rather than for a fixed time.
            game.page.keyboard.down("w")
            try:
                game.wait_for(f"s.players.some((p) => p.mine && Math.hypot(p.position[0] - {start[0]}, p.position[2] - {start[2]}) > 1.5)")
            finally:
                game.page.keyboard.up("w")
            rest = game.rest_position()
            self.assertGreater(distance(start, rest), 1.0, "walking forward didn't move the player")
            moved[game.own_player()["id"]] = rest

        third = self.open_game()
        third.join(code)
        state = third.wait_for("s.players.length === 3")
        for game in (host, second):
            game.wait_for("s.players.length === 3 && s.hud.includes('3 playing')")
        # Positions arrive a moment after the players do: looked for until
        # they have, rather than after a fixed wait.
        for player, position in moved.items():
            third.wait_for(f"s.players.some((p) => p.id === {player} && Math.hypot(p.position[0] - {position[0]}, p.position[2] - {position[2]}) < 0.1)")

    def test_a_joiner_leaving_is_removed_from_the_session(self):
        host = self.open_game()
        joiner = self.open_game()
        joiner.join(host.host())
        host.wait_for("s.players.length === 2")
        joiner.close()
        host.wait_for("s.players.length === 1 && s.hud.includes('1 playing')")


class PrivateSessionTest(GameTestCase):
    def test_a_private_session_is_not_listed_but_its_code_works(self):
        host = self.open_game()
        host.choose("PrivateButton", "Private: join by code only")
        code = host.host()

        other = self.open_game()
        other.click("RefreshButton")
        state = other.wait_for("s.sessions.length === 1")
        self.assertEqual(state["sessions"][0]["text"], NO_SESSIONS)

        other.join(code)
        other.wait_for(f"s.screen === 'game' && s.code === '{code}' && s.players.length === 2")

class LosingTheHostTest(GameTestCase):
    def test_every_joiner_is_told_when_the_host_closes_the_page(self):
        host = self.open_game()
        code = host.host()
        joiners = [self.open_game(), self.open_game()]
        for joiner in joiners:
            joiner.join(code)
        host.wait_for("s.players.length === 3")
        host.close()
        for joiner in joiners:
            # Without the session server's word that the host left, a joiner
            # would say only that the connection was lost. The time allowed
            # is for a busy machine; on its own it takes a second or two.
            state = joiner.wait_for("s.screen === 'disconnected'", timeout=15_000)
            self.assertEqual(state["notice"], "The host closed the session.")
            self.assertTrue(state["standings"].startswith("1. "), "how the session stood")

    def test_the_host_leaving_through_the_menu_ends_it_with_the_standings(self):
        host = self.open_game()
        joiner = self.open_game()
        joiner.join(host.host())
        joiner_id = joiner.own_player()["id"]
        host.wait_for("s.players.length === 2")
        host.page.wait_for_timeout(500)
        host.capture_mouse()
        host.shoot_until(f"s.players.some((p) => p.id === {joiner_id} && p.deaths >= 1)")
        host_name = host.own_player()["name"]

        host.press("Escape")
        host.click("MenuButton")
        host.click("MenuLeaveButton")
        state = joiner.wait_for("s.screen === 'disconnected'")
        self.assertEqual(state["notice"], "The host ended the session.")
        self.assertTrue(state["standings"].startswith(f"1. {host_name}  ·  "), state["standings"])
        self.assertNotIn("RejoinButton", state["controls"])
        host.wait_for("s.screen === 'menu'")

    def test_going_back_to_the_menu_after_losing_the_host(self):
        host = self.open_game()
        joiner = self.open_game()
        joiner.join(host.host())
        host.close()
        joiner.wait_for("s.screen === 'disconnected'")
        joiner.click("DisconnectedButton")
        state = joiner.wait_for("s.screen === 'menu'")
        self.assertEqual(state["status"], "")
        self.assertTrue(re.fullmatch(r"[A-Z]{4}", joiner.host()))


if __name__ == "__main__":
    unittest.main()
