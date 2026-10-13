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
        host.wait_for("s.combat.weapon === 'Curd Pistol'")
        host.press("5")
        host.wait_for("s.combat.weapon === 'Gouda Launcher'")
        host.page.mouse.wheel(0, 120)
        host.wait_for("s.combat.weapon === 'Curd Pistol'")
        host.page.mouse.wheel(0, -120)
        host.wait_for("s.combat.weapon === 'Gouda Launcher'")

    def test_a_weapon_fired_dry_gives_way_to_the_sidearm(self):
        host = self.open_game()
        host.host()
        self.walk_to_the_launcher(host)
        state = host.wait_for("s.combat.weapon === 'Gouda Launcher' && s.combat.reserve !== ''")
        self.assertEqual(state["combat"]["reserve"], "+ 4", "two reloads' worth besides the two loaded")
        host.capture_mouse()
        # Six wheels and two reloads later there is nothing left to fire.
        state = host.shoot_until("s.players[0].weapon === 'pistol'", timeout=120_000)
        own = state["players"][0]
        self.assertIn("launcher", own["carrying"], "still carried, for when there is ammunition")
        self.assertEqual(own["health"], 100, "none of them burst near enough to hurt")
        host.wait_for("s.combat.reserve === '+ ∞'")

    def test_a_thrown_grenade_bursts_and_is_gone_from_the_count(self):
        host = self.open_game()
        joiner = self.open_game()
        joiner.join(host.host())
        host.wait_for("s.players.length === 2 && s.players.every((p) => p.grenades === 2)")
        host.capture_mouse()
        host.press("g")
        host.wait_for("s.players.some((p) => p.mine && p.grenades === 1)")
        # Everyone sees it in the air, and then sees it gone.
        joiner.wait_for("s.projectiles >= 1")
        joiner.wait_for("s.projectiles === 0 && (s.sounds.burst || 0) >= 1")
        self.assertEqual(host.own_player()["health"], 100, "it burst well away from the thrower")

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
        hurt = "(p.health < 100 || p.deaths > 0)"
        host.shoot_until(f"s.players.some((p) => p.id === {joiner_id} && {hurt})")
        joiner.wait_for(f"s.players.some((p) => p.mine && {hurt}) && s.combat.hurts_shown >= 1")


class SoundTest(GameTestCase):
    def test_a_shot_is_heard_by_the_shooter_and_the_other_player(self):
        host = self.open_game()
        joiner = self.open_game()
        joiner.join(host.host())
        host.wait_for("s.players.length === 2")
        host.capture_mouse()
        host.hold_fire(50)
        host.wait_for("(s.sounds.blaster || 0) >= 1")
        joiner.wait_for("(s.sounds.blaster || 0) >= 1")

    def test_the_menu_has_music(self):
        game = self.open_game()
        game.wait_for("s.screen === 'menu' && s.music")


class OverloadTest(GameTestCase):
    def test_a_full_meter_sets_off_an_overload_on_q(self):
        host = self.open_game()
        # On the small arena, whoever is knocked out comes back straight ahead
        # of the host, the spot farthest from it.
        host.choose("MapButton", "Map: Brie Box")
        joiner = self.open_game()
        joiner.join(host.host())
        host.wait_for("s.players.length === 2")
        host.page.wait_for_timeout(500)
        host.capture_mouse()
        host.shoot_until("s.players.some((p) => p.mine && p.overload >= 100)")
        host.wait_for("s.combat.overload === 'Overload ready: Q'")
        host.press("q")
        state = host.wait_for("s.players.some((p) => p.mine && p.overloaded)")
        self.assertTrue(state["combat"]["overload"].startswith("OVERLOAD"))
        joiner.wait_for("s.players.some((p) => p.id === 1 && p.overloaded)")


if __name__ == "__main__":
    unittest.main()
