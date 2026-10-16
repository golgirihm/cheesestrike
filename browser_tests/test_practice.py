"""The practice range: a session alone, started from the menu, with targets
to shoot at and numbers for what each weapon has done."""

import unittest

from harness import GameTestCase


class PracticeTest(GameTestCase):
    def test_the_range_is_played_alone_with_targets_and_numbers(self):
        game = self.open_game()
        game.click("PracticeButton")
        state = game.wait_for("s.screen === 'game' && s.map === 'range' && s.players.length === 6")
        self.assertEqual(state["hud"], "Practice range")
        self.assertEqual(state["combat"]["match"], "Practice range")
        self.assertEqual(state["combat"]["practice"], [])
        names = sorted(player["name"] for player in state["players"] if not player["mine"])
        self.assertEqual(names, ["Target 10 m", "Target 100 m", "Target 25 m", "Target 50 m", "Target 75 m"])
        # The player starts facing down the range at the target in the middle.
        game.capture_mouse()
        state = game.shoot_until("s.players.some((p) => !p.mine && (p.health < 100 || p.deaths > 0))")
        self.assertRegex(state["combat"]["practice"][0], r"^Cheddar Blaster: \d+ fired · \d+% hit · \d+ damage")
        self.assertGreaterEqual(state["combat"]["hits_shown"], 1)

    def test_leaving_the_range_goes_back_to_the_menu(self):
        game = self.open_game()
        game.click("PracticeButton")
        game.wait_for("s.screen === 'game' && s.map === 'range'")
        game.open_menu()
        game.click("MenuLeaveButton")
        game.wait_for("s.screen === 'menu'")
        # And an ordinary session can be hosted after it.
        game.host()
        state = game.wait_for("s.screen === 'game' && s.players.length === 1")
        self.assertNotEqual(state["map"], "range")


if __name__ == "__main__":
    unittest.main()
