"""The settings screen, through the real menus."""

import unittest

from harness import GameTestCase


class SettingsTest(GameTestCase):
    def test_a_setting_changed_on_the_menu_is_kept_after_a_reload(self):
        game = self.open_game()
        game.click("SettingsButton")
        game.wait_for("s.screen === 'settings'")
        game.click("Setting_invert_look")
        game.wait_for("s.settings.invert_look === true")
        game.click("Setting_quality")
        game.wait_for("s.settings.quality !== 'high'")
        game.click("SettingsDone")
        game.wait_for("s.screen === 'menu'")
        # The browser stores the file a moment after the game writes it.
        game.page.wait_for_timeout(1500)
        game.page.reload()
        state = game.wait_for("s.screen === 'menu'")
        self.assertTrue(state["settings"]["invert_look"])
        self.assertNotEqual(state["settings"]["quality"], "high")

    def test_choosing_a_new_jump_key(self):
        game = self.open_game()
        game.click("SettingsButton")
        game.scroll_to("Key_jump")
        game.click("Key_jump")
        game.page.wait_for_timeout(200)
        game.press("j")
        game.wait_for("s.settings.jump_keys === 'J'")
        game.scroll_to("SettingsResetKeys")
        game.click("SettingsResetKeys")
        game.wait_for("s.settings.jump_keys === 'Space'")

    def test_settings_open_from_the_in_game_menu_and_go_back_to_it(self):
        game = self.open_game()
        game.host()
        game.click("MenuButton")
        game.click("PauseSettingsButton")
        game.wait_for("s.screen === 'settings'")
        game.click("SettingsDone")
        game.wait_for("s.screen === 'paused'")


if __name__ == "__main__":
    unittest.main()
