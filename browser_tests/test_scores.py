"""The kill feed and the scoreboard, seen by players who weren't in the fight."""

import unittest

from harness import GameTestCase


class ScoresTest(GameTestCase):
    def named_game(self, name):
        game = self.open_game()
        game.click("NameEdit")
        game.type(name)
        game.press("Enter")
        game.wait_for(f"s.name_box === {name!r}")
        return game

    def knock_out(self, host, victim_id):
        host.capture_mouse()
        host.shoot_until(f"s.players.some((p) => p.id === {victim_id} && p.deaths >= 1)")

    def test_a_knock_out_shows_in_everyones_kill_feed(self):
        host = self.named_game("Hunter")
        code = host.host()
        target = self.named_game("Target")
        target.join(code)
        target_id = target.own_player()["id"]
        watcher = self.named_game("Watcher")
        watcher.join(code)
        host.wait_for("s.players.length === 3")
        host.page.wait_for_timeout(500)

        self.knock_out(host, target_id)
        expected = "Hunter  [Curd Pistol"
        for game in (host, target, watcher):
            state = game.wait_for("s.combat.knockouts_announced >= 1")
            self.assertTrue(state["combat"]["latest_knockout"].startswith(expected), state["combat"]["latest_knockout"])
            self.assertTrue(state["combat"]["latest_knockout"].endswith("]  Target"))

    def test_holding_tab_shows_the_scoreboard(self):
        host = self.named_game("Hunter")
        code = host.host()
        target = self.named_game("Target")
        target.join(code)
        target_id = target.own_player()["id"]
        host.wait_for("s.players.length === 2")
        host.page.wait_for_timeout(500)
        self.knock_out(host, target_id)

        self.assertEqual(target.state()["combat"]["scoreboard"], [])
        target.page.keyboard.down("Tab")
        state = target.wait_for("s.combat.scoreboard.length === 3")
        heading, first, second = state["combat"]["scoreboard"]
        self.assertEqual(heading, ["Player", "Knock-outs", "Times out", "Ping"])
        self.assertEqual(first[:2], ["Hunter", "1"])
        self.assertEqual(second[0], "Target")
        self.assertGreaterEqual(int(second[2]), 1)
        target.page.keyboard.up("Tab")
        target.wait_for("s.combat.scoreboard.length === 0")


if __name__ == "__main__":
    unittest.main()
