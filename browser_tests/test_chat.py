"""Pings and text chat between teammates: seen by the team, not by the other
side, over real connections."""

import unittest

from harness import GameTestCase


class TeamTalkTest(GameTestCase):
    def three_in_teams(self):
        """A team match with three in it: the host and the last to join on
        one team, the one between them on the other."""
        host = self.open_game()
        host.choose("ModeButton", "Cheddar vs Brie")
        code = host.host()
        opponent = self.open_game()
        opponent.join(code)
        host.wait_for("s.players.length === 2")
        mate = self.open_game()
        mate.join(code)
        for game in (host, opponent, mate):
            game.wait_for("s.players.length === 3")
        teams = {player["id"]: player["team"] for player in host.state()["players"]}
        self.assertEqual(teams[mate.own_player()["id"]], teams[host.own_player()["id"]])
        self.assertNotEqual(teams[opponent.own_player()["id"]], teams[host.own_player()["id"]])
        return host, mate, opponent

    def test_a_teammate_sees_a_ping_and_an_opponent_does_not(self):
        host, mate, opponent = self.three_in_teams()
        host_id = host.own_player()["id"]
        self.assertEqual(mate.state()["pings"], [])
        host.capture_mouse()
        host.press("f")
        state = host.wait_for("s.pings.length === 1")
        self.assertEqual(state["pings"][0]["by"], host_id)
        state = mate.wait_for("s.pings.length === 1")
        self.assertEqual(state["pings"][0]["by"], host_id)
        self.assertIn("ping", state["compass"], "and it is on their compass")
        self.assertIn("teammate", state["compass"])
        opponent.page.wait_for_timeout(700)
        self.assertEqual(opponent.state()["pings"], [])
        self.assertNotIn("ping", opponent.state()["compass"])
        self.assertNotIn("teammate", opponent.state()["compass"], "nor does it show them where the others are")

    def test_chat_to_all_reaches_everyone_cleaned_and_team_chat_only_the_team(self):
        host, mate, opponent = self.three_in_teams()
        host.capture_mouse()
        before = host.own_player()
        host.press("Enter")
        host.wait_for("s.chat.typing")
        host.type("good luck you sh1t")
        host.press("Enter")
        host.wait_for("!s.chat.typing")
        for game in (host, mate, opponent):
            state = game.wait_for("s.chat.lines.length === 1")
            self.assertTrue(state["chat"]["lines"][0].endswith(": good luck you ****"), state["chat"]["lines"])
            self.assertFalse(state["chat"]["lines"][0].startswith("[Team]"))
        # To the team alone.
        host.press("y")
        host.wait_for("s.chat.typing")
        host.type("meet at the hill")
        host.press("Enter")
        state = mate.wait_for("s.chat.lines.length === 2")
        self.assertTrue(state["chat"]["lines"][1].startswith("[Team] "))
        self.assertTrue(state["chat"]["lines"][1].endswith(": meet at the hill"))
        opponent.page.wait_for_timeout(700)
        self.assertEqual(len(opponent.state()["chat"]["lines"]), 1, "the other team reads nothing of it")
        # Typing didn't walk the player about or fire.
        after = host.own_player()
        self.assertEqual(after["ammo"], before["ammo"])
        self.assertEqual(after["weapon"], before["weapon"])
        self.assertAlmostEqual(after["position"][0], before["position"][0], delta=0.2)
        self.assertAlmostEqual(after["position"][2], before["position"][2], delta=0.2)


if __name__ == "__main__":
    unittest.main()
