"""The in-game menu."""

import unittest

from harness import GameTestCase


class InGameMenuTest(GameTestCase):
    def test_esc_opens_it_and_resume_closes_it(self):
        game = self.open_game()
        game.host()
        state = game.open_menu()
        self.assertIn("ResumeButton", state["controls"])
        game.click("ResumeButton")
        game.wait_for("s.screen === 'game'")

    def test_esc_opens_it_at_once_even_with_the_mouse_taken_and_closes_it_again(self):
        game = self.open_game()
        game.host()
        game.capture_mouse()
        game.press("Escape")
        game.wait_for("s.screen === 'paused'")
        game.press("Escape")
        game.wait_for("s.screen === 'game'")

    def test_there_is_no_menu_button_on_the_screen(self):
        game = self.open_game()
        game.host()
        game.page.wait_for_timeout(300)
        self.assertNotIn("MenuButton", game.state()["controls"])

    def test_esc_goes_back_from_hosting_and_joining_to_the_title(self):
        game = self.open_game()
        game.press("Escape")
        game.wait_for("s.screen === 'title'")

    def test_leaving_from_the_menu_returns_to_the_main_menu(self):
        host = self.open_game()
        joiner = self.open_game()
        joiner.join(host.host())
        host.wait_for("s.players.length === 2")
        joiner.open_menu()
        joiner.click("MenuLeaveButton")
        state = joiner.wait_for("s.screen === 'menu'")
        self.assertEqual(state["status"], "")
        host.wait_for("s.players.length === 1")


class ControlsHelpTest(GameTestCase):
    def test_a_first_session_shows_the_controls_once(self):
        game = self.open_game("&help")
        game.click("HostButton")
        state = game.wait_for("s.screen === 'help'")
        self.assertIn("Fire: Left mouse button", state["help"])
        game.click("HelpDone")
        game.wait_for("s.screen === 'game'")
        # Shown, the player can play: clicking the game takes the mouse.
        game.capture_mouse()

        game.open_menu()
        game.click("MenuLeaveButton")
        game.wait_for("s.screen === 'menu'")
        game.host()
        game.page.wait_for_timeout(500)
        self.assertEqual(game.state()["screen"], "game")

    def test_the_controls_can_be_shown_again_from_the_menu(self):
        game = self.open_game()
        game.host()
        game.open_menu()
        game.click("HelpButton")
        state = game.wait_for("s.screen === 'help'")
        self.assertIn("Menu: Esc", state["help"])
        game.press("Escape")
        game.wait_for("s.screen === 'paused'")


if __name__ == "__main__":
    unittest.main()
