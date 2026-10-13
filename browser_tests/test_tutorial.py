"""The tutorial, walked through from the menu to its last step the way a
player with a keyboard and mouse would."""

import unittest

from harness import GameTestCase


class TutorialTest(GameTestCase):
    def step(self, game, number):
        return game.wait_for(f"s.combat.tutorial.startsWith('{number} of 7')")

    def test_a_player_is_taken_through_every_step(self):
        game = self.open_game()
        game.click("TutorialButton")
        game.wait_for("s.screen === 'game' && s.map === 'range'")
        state = self.step(game, 1)
        self.assertIn("Walk about", state["combat"]["tutorial"])
        self.assertEqual(state["help"], [], "it says how itself, without the controls first")

        # Walk a little way toward the rack, not as far as it.
        game.page.keyboard.down("w")
        try:
            self.step(game, 2)
        finally:
            game.page.keyboard.up("w")

        # Look: take the mouse and move it, then put the view back.
        game.capture_mouse()
        facing = game.own_player()["facing"][0]
        for x in (30, 60, 90, 120, 150, 180):
            game.page.mouse.move(x, 1)
            game.page.wait_for_timeout(50)
        self.step(game, 3)
        for x in (150, 120, 90, 60, 30, 1):
            game.page.mouse.move(x, 1)
            game.page.wait_for_timeout(50)
        game.page.mouse.move(0, 0)

        game.hold_fire(100)
        self.step(game, 4)

        game.page.mouse.down(button="right")
        try:
            self.step(game, 5)
        finally:
            game.page.mouse.up(button="right")

        game.press("r")
        self.step(game, 6)

        # On to the rack: a weapon lies straight ahead of where a player
        # starts, if they are still facing about the way they were.
        self.assertAlmostEqual(game.own_player()["facing"][0], facing, delta=0.25)
        game.page.keyboard.down("w")
        try:
            state = self.step(game, 7)
        finally:
            game.page.keyboard.up("w")
        self.assertGreater(len(game.own_player()["carrying"]), 2)
        self.assertIn("Overload is full", state["combat"]["tutorial"])
        game.wait_for("s.combat.overload.startsWith('Overload ready')")

        game.press("q")
        game.wait_for("s.combat.tutorial.startsWith(\"That's everything\")")
        self.assertTrue(game.own_player()["overloaded"])


if __name__ == "__main__":
    unittest.main()
