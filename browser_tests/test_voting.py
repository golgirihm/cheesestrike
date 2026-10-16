"""Voting: on what to play next when a match ends, and on removing a player."""

import unittest

from harness import GameTestCase


class VotingTest(GameTestCase):
    def test_everyone_votes_for_the_next_match_and_gets_it(self):
        # A short match: one hit knocks out, and on the small arena whoever
        # is knocked out comes back straight ahead of the host.
        host = self.open_game()
        host.choose("RulesButton", "Rules: Instant knock-out")
        host.choose("KillLimitButton", "First to 5")
        host.choose("MapButton", "Map: Brie Box")
        code = host.host()
        joiner = self.open_game()
        joiner.join(code)
        host.wait_for("s.players.length === 2")
        host.page.wait_for_timeout(500)
        host.capture_mouse()
        own_name = host.own_player()["name"]
        host.shoot_until(f"s.combat.result === {own_name + ' wins!'!r}", timeout=180_000)

        for game in (host, joiner):
            state = game.wait_for("s.combat.vote.length === 4")
            self.assertEqual(state["combat"]["vote"][0], "Vote for the next match")
            self.assertEqual(state["combat"]["vote"][3], "F3   The same again   (0)")
        self.assertIn("Curd City  ·  Cheddar vs Brie", host.state()["combat"]["vote"][1])
        joiner.press("F1")
        host.wait_for("s.combat.vote.length === 4 && s.combat.vote[1].endsWith('(1)')")
        host.press("F1")
        joiner.wait_for("s.combat.vote.length === 4 && s.combat.vote[1].endsWith('(2)')")

        # After the break: the arena and the mode they chose, by the same rules.
        for game in (host, joiner):
            game.wait_for("s.map === 'city' && s.combat.result === '' && s.combat.match.startsWith('Cheddar 0') && s.combat.vote.length === 0 && s.combat.health === '1'", timeout=90_000)
            self.assertEqual(sorted(player["team"] for player in game.state()["players"]), [0, 1])

    def test_a_player_is_voted_out_and_stays_out(self):
        host = self.open_game()
        code = host.host()
        asker = self.open_game()
        asker.join(code)
        unwanted = self.open_game()
        unwanted.join(code)
        # A player is in the session a moment before the host has their name.
        for game in (host, asker, unwanted):
            game.wait_for("s.players.length === 3 && s.players.every((p) => p.name !== 'Player')")
        unwanted_name = unwanted.own_player()["name"]

        # The only one the asker can name: not themselves, and not the host.
        asker.click("MenuButton")
        state = asker.wait_for("'RemoveVoteButton' in s.controls")
        asker.click("RemoveVoteButton")
        for game in (host, unwanted):
            state = game.wait_for("s.combat.vote.length === 3")
            self.assertTrue(state["combat"]["vote"][0].startswith(f"Remove {unwanted_name}?   2 needed"), state["combat"]["vote"])
            self.assertEqual(state["combat"]["vote"][1], "F1   Yes   (1)")
        # They have no say in it; the host's yes settles it.
        unwanted.press("F2")
        host.page.wait_for_timeout(300)
        self.assertEqual(host.state()["combat"]["vote"][2], "F2   No   (0)")
        host.press("F1")

        state = unwanted.wait_for("s.notice !== ''")
        self.assertEqual(state["notice"], "You were removed from the session by a vote.")
        self.assertNotIn("RejoinButton", state["controls"], "and isn't offered a way back")
        for game in (host, asker):
            game.wait_for("s.players.length === 2 && s.combat.vote.length === 0")

        # Coming back by the code gets them nowhere.
        unwanted.click("DisconnectedButton")
        unwanted.wait_for("s.screen === 'menu'")
        unwanted.click("TargetEdit")
        # Over the code that is still there from the first time.
        unwanted.page.keyboard.press("Control+A")
        unwanted.type(code)
        unwanted.press("Enter")
        unwanted.wait_for("s.notice === 'You were removed from the session by a vote.'")
        # The host has them for a moment, until it knows who they are.
        host.wait_for("s.players.length === 2")
        host.page.wait_for_timeout(500)
        self.assertEqual(len(host.state()["players"]), 2)
        self.assertEqual(unwanted.state()["screen"], "disconnected")


if __name__ == "__main__":
    unittest.main()
