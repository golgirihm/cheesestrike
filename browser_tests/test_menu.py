"""The in-game menu."""

import unittest

from harness import GameTestCase


class InGameMenuTest(GameTestCase):
    def test_the_menu_button_opens_it_and_resume_closes_it(self):
        game = self.open_game()
        game.host()
        game.click("MenuButton")
        state = game.wait_for("s.screen === 'paused'")
        self.assertIn("ResumeButton", state["controls"])
        self.assertNotIn("MenuButton", state["controls"], "the button hides while the menu is open")
        game.click("ResumeButton")
        game.wait_for("s.screen === 'game'")

    def test_esc_opens_and_closes_it_once_the_mouse_is_free(self):
        game = self.open_game()
        game.host()
        game.page.wait_for_timeout(200)
        game.press("Escape")
        game.wait_for("s.screen === 'paused'")
        game.press("Escape")
        game.wait_for("s.screen === 'game'")

    def test_the_menu_button_hides_while_the_game_has_the_mouse(self):
        game = self.open_game()
        game.host()
        game.wait_for("'MenuButton' in s.controls")
        game.capture_mouse()
        game.wait_for("!('MenuButton' in s.controls)")

    def test_leaving_from_the_menu_returns_to_the_main_menu(self):
        host = self.open_game()
        joiner = self.open_game()
        joiner.join(host.host())
        host.wait_for("s.players.length === 2")
        joiner.click("MenuButton")
        joiner.click("MenuLeaveButton")
        state = joiner.wait_for("s.screen === 'menu'")
        self.assertEqual(state["status"], "")
        host.wait_for("s.players.length === 1")


if __name__ == "__main__":
    unittest.main()
