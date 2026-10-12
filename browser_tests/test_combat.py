"""Shooting, through real mouse clicks and a real connection: the host of a
session starts on the first spot and the first joiner on the second, facing
each other across the arena, so a shot fired down the sights without turning
hits. Everyone starts with the Curd Pistol, which fires once for each pull.

A slow machine can run the game with long gaps between frames, so these wait
for what lasts (a score, a count of reloads) rather than for a passing moment
such as the seconds a player spends knocked out."""

import unittest

from harness import GameTestCase

FULL_HEALTH = 100
MAGAZINE = 10


class CombatTest(GameTestCase):
    def start_pair(self):
        """A host and one joiner, both in the arena. Returns the host, the
        joiner and the joiner's player id."""
        host = self.open_game()
        joiner = self.open_game()
        joiner.join(host.host())
        joiner_id = joiner.own_player()["id"]
        host.wait_for(f"s.players.length === 2 && s.players.every((p) => p.health === {FULL_HEALTH})")
        # Let the joiner land and its position reach the host.
        host.page.wait_for_timeout(500)
        return host, joiner, joiner_id

    def test_the_click_that_takes_the_mouse_does_not_fire(self):
        host = self.open_game()
        host.host()
        host.capture_mouse()
        host.page.wait_for_timeout(300)
        self.assertEqual(host.own_player()["ammo"], MAGAZINE)

    def test_taking_the_mouse_and_clicking_leave_the_view_where_it_was(self):
        host = self.open_game()
        host.host()
        facing = host.own_player()["facing"]
        host.capture_mouse()
        host.hold_fire(50)
        host.wait_for(f"s.players[0].ammo < {MAGAZINE}")
        # Within a degree: the test tool's pointer sits a pixel from the
        # corner, which may still turn the view a hair (see capture_mouse).
        for now, before in zip(host.own_player()["facing"], facing):
            self.assertAlmostEqual(now, before, delta=0.02)

    def test_a_shot_hurts_the_player_it_hits_in_everyones_view(self):
        host, joiner, joiner_id = self.start_pair()
        host.capture_mouse()
        # A slow machine may fire several shots before it is seen, enough to
        # knock the player out and bring them back, so a knock-out counts too.
        hurt = f"(p.health < {FULL_HEALTH} || p.deaths > 0)"
        state = host.shoot_until(f"s.players.some((p) => p.id === {joiner_id} && {hurt})")
        joiner.wait_for(f"s.players.some((p) => p.mine && {hurt})")
        own = next(player for player in state["players"] if player["mine"])
        self.assertEqual(own["health"], FULL_HEALTH, "the shooter isn't hurt")

    def test_the_shooter_sees_a_hit_marker_and_the_one_hit_sees_where_from(self):
        host, joiner, joiner_id = self.start_pair()
        host.capture_mouse()
        host.shoot_until("s.combat.hits_shown >= 1")
        state = joiner.wait_for("s.combat.hurts_shown >= 1")
        self.assertEqual(state["combat"]["hits_shown"], 0)
        self.assertEqual(host.state()["combat"]["hurts_shown"], 0)

    def test_health_and_ammo_are_on_screen(self):
        host = self.open_game()
        host.host()
        state = host.wait_for("s.combat.ammo !== ''")
        self.assertEqual(state["combat"]["health"], str(FULL_HEALTH))
        self.assertEqual(state["combat"]["ammo"], f"{MAGAZINE} / {MAGAZINE}")
        host.capture_mouse()
        host.hold_fire(50)
        host.wait_for(f"s.combat.ammo !== '{MAGAZINE} / {MAGAZINE}'")

    def test_enough_hits_knock_a_player_out_and_score(self):
        host, joiner, joiner_id = self.start_pair()
        host.capture_mouse()
        host.shoot_until(f"s.players.some((p) => p.id === {joiner_id} && p.deaths >= 1)")
        host.wait_for("s.players.some((p) => p.mine && p.kills >= 1)")
        joiner.wait_for("s.players.some((p) => p.mine && p.deaths >= 1) && s.players.some((p) => !p.mine && p.kills >= 1)")

    def test_a_knocked_out_player_comes_back_with_full_health(self):
        host, joiner, joiner_id = self.start_pair()
        host.capture_mouse()
        host.shoot_until(f"s.players.some((p) => p.id === {joiner_id} && p.deaths >= 1)")
        joiner.wait_for(f"s.players.some((p) => p.mine && p.deaths >= 1 && p.alive && p.health === {FULL_HEALTH})")

    def test_holding_the_right_button_aims_down_the_sights(self):
        host = self.open_game()
        host.host()
        host.capture_mouse()
        self.assertFalse(host.own_player()["aiming"])
        host.page.mouse.down(button="right")
        host.wait_for("s.players[0].aiming")
        host.page.mouse.up(button="right")
        host.wait_for("!s.players[0].aiming")

    def test_reloading_fills_the_magazine(self):
        host = self.open_game()
        host.host()
        host.capture_mouse()
        host.hold_fire(100)
        host.wait_for(f"s.players[0].ammo < {MAGAZINE}")
        host.press("r")
        state = host.wait_for("s.players[0].reloads === 1")
        self.assertEqual(state["players"][0]["ammo"], MAGAZINE)

    def test_emptying_the_magazine_reloads_by_itself(self):
        host = self.open_game()
        host.host()
        host.capture_mouse()
        host.shoot_until("s.players[0].reloads === 1")


if __name__ == "__main__":
    unittest.main()
