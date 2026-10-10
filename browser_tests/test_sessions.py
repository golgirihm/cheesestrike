"""Hosting, finding, joining and losing sessions, through the real menu and a
real WebRTC connection between browser windows."""

import math
import re
import unittest

from harness import GameTestCase

NO_SESSIONS = "None yet. Host one!"


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

        self.signaling.start()
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
        listed = f"{code}  ·  1 playing"
        state = joiner.wait_for(f"s.sessions.length === 1 && s.sessions[0].text === '{listed}'")
        joiner.click_at(state["sessions"][0]["at"])
        for game in (host, joiner):
            game.wait_for("s.screen === 'game' && s.players.length === 2 && s.hud.includes('2 playing')")

    def test_a_closed_session_leaves_the_list(self):
        host = self.open_game()
        code = host.host()
        other = self.open_game()
        other.wait_for(f"s.sessions.length === 1 && s.sessions[0].text.startsWith('{code}')")
        host.close()
        # The list is only asked for on request, so keep asking until it's gone.
        other.click("RefreshButton")
        other.wait_for(f"s.sessions.length === 1 && s.sessions[0].text === '{NO_SESSIONS}'")

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
            game.page.keyboard.down("w")
            game.page.wait_for_timeout(600)
            game.page.keyboard.up("w")
            game.page.wait_for_timeout(300)
            player = game.own_player()
            self.assertGreater(distance(start, player["position"]), 1.0, "walking forward didn't move the player")
            moved[player["id"]] = player["position"]

        third = self.open_game()
        third.join(code)
        state = third.wait_for("s.players.length === 3")
        for game in (host, second):
            game.wait_for("s.players.length === 3 && s.hud.includes('3 playing')")
        # Positions arrive a moment after the players do.
        third.page.wait_for_timeout(500)
        seen = {player["id"]: player["position"] for player in third.state()["players"]}
        for player, position in moved.items():
            self.assertLess(distance(seen[player], position), 0.1, f"player {player} is shown in the wrong place")

    def test_a_joiner_leaving_is_removed_from_the_session(self):
        host = self.open_game()
        joiner = self.open_game()
        joiner.join(host.host())
        host.wait_for("s.players.length === 2")
        joiner.close()
        host.wait_for("s.players.length === 1 && s.hud.includes('1 playing')")


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
            # Well inside the time a dropped connection takes to be noticed on
            # its own, so this passes only if the session server's word that
            # the host left got through.
            state = joiner.wait_for("s.screen === 'disconnected'", timeout=5_000)
            self.assertEqual(state["notice"], "The host closed the session.")

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
