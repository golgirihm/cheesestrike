"""The ways to play beyond knock-outs, each played far enough to score in it.

On Brie Box the host starts in one corner, on Cheddar's base, facing the
middle of the arena, and the first to join starts in the opposite corner, on
Brie's. The middle is the first hill and the second of the three points. So
walking forward and back is all a player needs to do to score."""

import unittest

from harness import GameTestCase

NEAR_THE_MIDDLE = "s.players.some((p) => p.mine && Math.hypot(p.position[0], p.position[2]) < 3)"


class ModesTest(GameTestCase):
    def start(self, mode, map="Brie Box"):
        host = self.open_game()
        host.choose("ModeButton", mode)
        host.choose("MapButton", f"Map: {map}")
        code = host.host()
        joiner = self.open_game()
        joiner.join(code)
        host.wait_for("s.players.length === 2")
        # Let the joiner land and its position reach the host.
        host.page.wait_for_timeout(500)
        return host, joiner

    def walk(self, game, key, until):
        game.page.keyboard.down(key)
        try:
            return game.wait_for(until)
        finally:
            game.page.keyboard.up(key)

    def test_a_wheel_carried_home_scores(self):
        host, joiner = self.start("Capture the wheel")
        self.assertEqual(host.state()["combat"]["objective"], "Cheddar wheel home · Brie wheel home")
        # The joiner steps off their base, or they would be in the way.
        self.walk(joiner, "a", "s.players.some((p) => p.mine && Math.hypot(p.position[0] - 12, p.position[2] - 12) > 5)")
        self.walk(host, "w", "s.combat.objective.includes('Brie wheel with')")
        joiner.wait_for("s.combat.objective.includes('Brie wheel with') && s.combat.announcement.includes('has the Brie wheel')")
        # Back the way they came, to their own base.
        self.walk(host, "s", "s.combat.points[0] === 1")
        for game in (host, joiner):
            game.wait_for("s.combat.points[0] === 1 && s.combat.objective === 'Cheddar wheel home · Brie wheel home' && s.combat.match.startsWith('Cheddar 1')")

    def test_standing_on_the_hill_scores(self):
        host, joiner = self.start("Hold the hill")
        self.assertEqual(host.state()["combat"]["points"], [0, 0])
        self.walk(host, "w", NEAR_THE_MIDDLE)
        for game in (host, joiner):
            game.wait_for("s.combat.points[0] >= 2 && s.combat.points[1] === 0 && s.combat.objective.startsWith('The hill moves in')")

    def test_a_point_stood_in_is_taken_and_scores(self):
        host, joiner = self.start("Three points")
        self.assertEqual(host.state()["combat"]["objective"], "A – · B – · C –")
        self.walk(host, "w", NEAR_THE_MIDDLE)
        for game in (host, joiner):
            game.wait_for("s.combat.objective === 'A – · B Cheddar · C –' && s.combat.points[0] >= 1")

    def test_a_round_goes_to_the_last_team_standing_and_then_everyone_is_back(self):
        host, joiner = self.start("Last mouse standing", map="Cheddar Canyon")
        joiner_id = joiner.own_player()["id"]
        host.capture_mouse()
        host.shoot_until(f"s.players.some((p) => p.id === {joiner_id} && p.deaths >= 1)")
        for game in (host, joiner):
            game.wait_for("s.combat.points[0] === 1")
        joiner.wait_for("s.players.some((p) => p.mine && p.alive && p.deaths >= 1) && !s.combat.objective.startsWith('Next round')")

    def test_a_knock_out_in_a_gun_game_changes_the_weapon_in_hand(self):
        host, joiner = self.start("Gun game", map="Cheddar Canyon")
        joiner_id = joiner.own_player()["id"]
        host_id = host.own_player()["id"]
        self.assertEqual(host.state()["combat"]["weapon"], "Cheddar Blaster")
        self.assertEqual(host.own_player()["carrying"], ["blaster"])
        host.capture_mouse()
        host.shoot_until(f"s.players.some((p) => p.id === {joiner_id} && p.deaths >= 1)")
        host.wait_for("s.combat.weapon === 'Swiss Shotgun' && s.combat.objective.startsWith('Weapon 2 of')")
        joiner.wait_for(f"s.players.some((p) => p.id === {host_id} && p.holding === 'shotgun')")


if __name__ == "__main__":
    unittest.main()
