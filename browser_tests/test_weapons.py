"""Picking up weapons and using them. The arena's Gouda Launcher sits in the
middle, straight ahead of the host's starting spot."""

import unittest

from harness import GameTestCase


class WeaponsTest(GameTestCase):
    def walk_to_the_launcher(self, game):
        game.page.keyboard.down("w")
        try:
            game.wait_for("s.players.some((p) => p.mine && p.carrying.includes('launcher'))")
        finally:
            game.page.keyboard.up("w")

    def test_walking_onto_a_pad_picks_up_its_weapon(self):
        host = self.open_game()
        host.host()
        self.walk_to_the_launcher(host)
        state = host.wait_for("s.combat.weapon === 'Gouda Launcher'")
        own = next(player for player in state["players"] if player["mine"])
        self.assertEqual(own["weapon"], "launcher", "a new weapon comes into hand")
        self.assertGreaterEqual(state["combat"]["pickups_shown"], 1)
        self.assertIn(False, state["pickups"], "the pad is empty now")

    def test_number_keys_and_the_wheel_change_weapons(self):
        host = self.open_game()
        host.host()
        self.walk_to_the_launcher(host)
        host.capture_mouse()
        host.press("1")
        host.wait_for("s.combat.weapon === 'Cheddar Blaster'")
        host.press("4")
        host.wait_for("s.combat.weapon === 'Gouda Launcher'")
        host.page.mouse.wheel(0, 120)
        host.wait_for("s.combat.weapon === 'Cheddar Blaster'")
        host.page.mouse.wheel(0, -120)
        host.wait_for("s.combat.weapon === 'Gouda Launcher'")

    def test_others_see_the_weapon_in_hand(self):
        host = self.open_game()
        joiner = self.open_game()
        joiner.join(host.host())
        self.walk_to_the_launcher(host)
        joiner.wait_for("s.players.some((p) => p.id === 1 && p.holding === 'launcher')")

    def test_a_launcher_wheel_hurts_the_player_it_bursts_by(self):
        host = self.open_game()
        joiner = self.open_game()
        joiner.join(host.host())
        joiner_id = joiner.own_player()["id"]
        host.wait_for("s.players.length === 2")
        self.walk_to_the_launcher(host)
        host.capture_mouse()
        host.page.mouse.down()
        hurt = "(p.health < 100 || p.deaths > 0)"
        host.wait_for(f"s.players.some((p) => p.id === {joiner_id} && {hurt})")
        host.page.mouse.up()
        joiner.wait_for(f"s.players.some((p) => p.mine && {hurt}) && s.combat.hurts_shown >= 1")


if __name__ == "__main__":
    unittest.main()
