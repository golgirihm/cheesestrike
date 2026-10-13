"""The settings screen, through the real menus."""

import unittest

from harness import GameTestCase


class SettingsTest(GameTestCase):
    def test_a_setting_changed_on_the_menu_is_kept_after_a_reload(self):
        game = self.open_game()
        game.click("SettingsButton")
        game.wait_for("s.screen === 'settings'")
        game.scroll_to("Setting_invert_look")
        game.click("Setting_invert_look")
        game.wait_for("s.settings.invert_look === true")
        game.scroll_to("Setting_quality")
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

    def test_others_see_the_character_a_player_chose(self):
        host = self.open_game()
        code = host.host()
        joiner = self.open_game()
        # Chosen on the menu, before joining.
        joiner.click("SettingsButton")
        joiner.wait_for("s.screen === 'settings'")
        joiner.scroll_to("Setting_look_hat")
        joiner.click("Setting_look_hat")
        joiner.click("Setting_look_hat")
        joiner.scroll_to("Setting_look_cheese")
        joiner.click("Setting_look_cheese")
        joiner.click("SettingsDone")
        joiner.wait_for("s.screen === 'menu'")
        joiner.join(code)
        joiner_id = joiner.own_player()["id"]
        look = host.player(joiner_id)["look"]
        host.wait_for(f"s.players.some((p) => p.id === {joiner_id} && p.look.hat === 'top' && p.look.cheese === 'cheddar')")
        self.assertEqual(host.player(1)["look"], {"cheese": "swiss", "hat": "none", "held": "none"}, "the host's own is as it was")
        self.assertIsNotNone(look)
        # Changed again in the middle of a session: it shows at once.
        joiner.click("MenuButton")
        joiner.click("PauseSettingsButton")
        joiner.wait_for("s.screen === 'settings'")
        joiner.scroll_to("Setting_look_held")
        joiner.click("Setting_look_held")
        host.wait_for(f"s.players.some((p) => p.id === {joiner_id} && p.look.held === 'balloon')")

    def test_settings_open_from_the_in_game_menu_and_go_back_to_it(self):
        game = self.open_game()
        game.host()
        game.click("MenuButton")
        game.click("PauseSettingsButton")
        game.wait_for("s.screen === 'settings'")
        game.click("SettingsDone")
        game.wait_for("s.screen === 'paused'")


class CaptionsTest(GameTestCase):
    def test_turned_on_captions_say_where_another_player_is_walking(self):
        host = self.open_game()
        host.host()
        host.click("MenuButton")
        host.click("PauseSettingsButton")
        host.scroll_to("Setting_captions")
        host.click("Setting_captions")
        host.wait_for("s.settings.captions === true")
        host.click("SettingsDone")
        host.click("ResumeButton")

        joiner = self.open_game()
        joiner.join(host.state()["code"])
        host.wait_for("s.players.length === 2")
        joiner.page.keyboard.down("w")
        state = host.wait_for("s.combat.captions.some((c) => c.startsWith('Footsteps, '))")
        joiner.page.keyboard.up("w")
        self.assertRegex(next(c for c in state["combat"]["captions"] if c.startswith("Footsteps")), r"^Footsteps, (ahead|behind|left|right)$")


if __name__ == "__main__":
    unittest.main()
