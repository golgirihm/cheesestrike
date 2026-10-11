"""Shooting, through real mouse clicks and a real connection: the host of a
session starts on the first spot and the first joiner on the second, facing
each other across the arena, so a shot fired without aiming hits.

A slow machine can run the game with long gaps between frames, so these wait
for what lasts (a score, a count of reloads) rather than for a passing moment
such as the seconds a player spends knocked out."""

import unittest

from harness import GameTestCase

FULL_HEALTH = 100
MAGAZINE = 24


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
        self.assertEqual(host.own_player()["facing"], facing)

    def test_a_shot_hurts_the_player_it_hits_in_everyones_view(self):
        host, joiner, joiner_id = self.start_pair()
        host.capture_mouse()
        host.hold_fire(50)
        state = host.wait_for(f"s.players.some((p) => p.id === {joiner_id} && p.health < {FULL_HEALTH})")
        joiner.wait_for(f"s.players.some((p) => p.mine && p.health < {FULL_HEALTH})")
        own = next(player for player in state["players"] if player["mine"])
        self.assertEqual(own["health"], FULL_HEALTH, "the shooter isn't hurt")

    def test_enough_hits_knock_a_player_out_and_score(self):
        host, joiner, joiner_id = self.start_pair()
        host.capture_mouse()
        host.page.mouse.down()
        host.wait_for(f"s.players.some((p) => p.id === {joiner_id} && p.deaths === 1)")
        host.page.mouse.up()
        host.wait_for("s.players.some((p) => p.mine && p.kills === 1)")
        joiner.wait_for("s.players.some((p) => p.mine && p.deaths === 1) && s.players.some((p) => !p.mine && p.kills === 1)")

    def test_a_knocked_out_player_comes_back_with_full_health(self):
        host, joiner, joiner_id = self.start_pair()
        host.capture_mouse()
        host.page.mouse.down()
        host.wait_for(f"s.players.some((p) => p.id === {joiner_id} && p.deaths === 1)")
        host.page.mouse.up()
        joiner.wait_for(f"s.players.some((p) => p.mine && p.deaths === 1 && p.alive && p.health === {FULL_HEALTH})")

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
        host.page.mouse.down()
        host.wait_for("s.players[0].reloads === 1")
        host.page.mouse.up()


if __name__ == "__main__":
    unittest.main()
