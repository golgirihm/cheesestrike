"""The front end: the title screen and the paths from it, the bar that fills
as the game gets ready, and the screen shown while a session is joined."""

import unittest

from harness import GameTestCase


class FrontEndTest(GameTestCase):
    def open_at_title(self):
        game = self.open_link(f"http://localhost:{self._web.server_address[1]}/?test&signal=ws://localhost:{self.signaling.port}")
        game.wait_for("s.screen === 'title'")
        return game

    def test_the_game_opens_on_its_title_and_play_leads_to_hosting_and_back(self):
        game = self.open_at_title()
        state = game.state()
        for way in ("PlayButton", "PracticeButton", "TutorialButton", "CustomiseButton", "EditorButton", "TitleSettingsButton", "CreditsButton"):
            self.assertIn(way, state["controls"])
        self.assertNotIn("HostButton", state["controls"])
        game._click("PlayButton")
        state = game.wait_for("s.screen === 'menu' && 'HostButton' in s.controls")
        self.assertNotIn("PlayButton", state["controls"])
        game._click("BackButton")
        game.wait_for("s.screen === 'title' && 'PlayButton' in s.controls")
        # And from Play a session can be hosted.
        game.host()
        game.wait_for("s.screen === 'game' && s.players.length === 1")

    def test_customise_settings_and_credits_each_open_and_come_back(self):
        game = self.open_at_title()
        game._click("CustomiseButton")
        state = game.wait_for("s.screen === 'settings' && 'Setting_look_hat' in s.controls")
        self.assertNotIn("Setting_fov", state["controls"], "only the character and weapons")
        game._click("SettingsDone")
        game.wait_for("s.screen === 'title'")
        game._click("TitleSettingsButton")
        state = game.wait_for("s.screen === 'settings' && 'Setting_fov' in s.controls")
        self.assertNotIn("Setting_look_hat", state["controls"], "the rest, without them")
        game._click("SettingsDone")
        game.wait_for("s.screen === 'title'")
        game._click("CreditsButton")
        state = game.wait_for("s.screen === 'credits'")
        self.assertNotIn("PlayButton", state["controls"])
        game._click("CreditsBackButton")
        game.wait_for("s.screen === 'title' && 'PlayButton' in s.controls")

    def test_the_practice_range_and_the_editor_are_a_press_from_the_title(self):
        game = self.open_at_title()
        game._click("PracticeButton")
        game.wait_for("s.screen === 'game' && s.map === 'range'")
        game.open_menu()
        game.click("MenuLeaveButton")
        game.wait_for("s.screen === 'menu'")
        game._click("BackButton")
        game.wait_for("s.screen === 'title'")
        game._click("EditorButton")
        game.wait_for("s.screen === 'editor'")

    def test_the_bar_fills_as_the_game_gets_ready_and_then_goes(self):
        game = self.open_at_title()
        seen = game.state()["loading"]
        self.assertGreaterEqual(len(seen), 3, "it moved")
        self.assertEqual(seen, sorted(seen), "only ever forward")
        self.assertLess(seen[0], 0.5, "from near empty")
        self.assertEqual(seen[-1], 1.0, "to full")
        self.assertNotEqual(game.state()["screen"], "loading", "and then it is gone")

    def test_joining_shows_the_arena_and_a_tip_until_the_arena_is_there(self):
        host = self.open_game()
        host.choose("MapButton", "Map: Brie Box")
        code = host.host()
        joiner = self.open_game()
        joiner.click("TargetEdit")
        joiner.type(code)
        joiner.press("Enter")
        # The screen is up from the press until the joiner is in the arena.
        state = joiner.wait_for("s.screen === 'joining' || s.screen === 'game'")
        if state["screen"] == "joining":
            self.assertTrue(state["joining"]["tip"])
            self.assertIn(state["joining"]["arena"], ("Brie Box", "Finding the session…"))
        state = joiner.wait_for("s.screen === 'game' && s.map === 'box'")
        self.assertEqual(state["joining"], {}, "gone once there")


if __name__ == "__main__":
    unittest.main()
