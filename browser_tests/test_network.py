"""How the connection holds up: pings, and a session with several players."""

import re
import unittest

from harness import GameTestCase


class NetworkTest(GameTestCase):
    def test_the_scoreboard_shows_each_players_ping(self):
        host = self.open_game()
        joiner = self.open_game()
        joiner.join(host.host())
        host.wait_for("s.players.length === 2")
        host.page.keyboard.down("Tab")
        try:
            # The host pings once a second.
            state = host.wait_for("s.combat.scoreboard.length === 3 && s.combat.scoreboard.slice(1).some((row) => row[3].endsWith(' ms'))", timeout=20_000)
        finally:
            host.page.keyboard.up("Tab")
        pings = [row[3] for row in state["combat"]["scoreboard"][1:]]
        self.assertIn("host", pings)
        measured = next(ping for ping in pings if ping != "host")
        self.assertRegex(measured, r"^\d+ ms$")
        self.assertLess(int(re.match(r"\d+", measured).group()), 2000)

    def test_five_players_all_see_each_other_move(self):
        host = self.open_game()
        code = host.host()
        joiners = []
        for _ in range(4):
            joiner = self.open_game()
            joiner.join(code)
            joiners.append(joiner)
        everyone = [host] + joiners
        for game in everyone:
            game.wait_for("s.players.length === 5 && s.hud.includes('5 playing')", timeout=60_000)

        mover = joiners[-1]
        mover_id = mover.own_player()["id"]
        start = mover.own_player()["position"]
        mover.page.keyboard.down("w")
        mover.page.wait_for_timeout(800)
        mover.page.keyboard.up("w")
        moved = mover.own_player()["position"]
        self.assertNotEqual(moved, start)
        for game in everyone[:-1]:
            game.wait_for(
                f"s.players.some((p) => p.id === {mover_id} && Math.hypot(p.position[0] - {start[0]}, p.position[2] - {start[2]}) > 1)",
                timeout=20_000,
            )
        self.assertEqual(host.state()["put_backs"], 0, "ordinary movement was taken for cheating")

    def test_running_jumping_and_being_shoved_are_never_taken_for_cheating(self):
        host = self.open_game()
        host.choose("MapButton", "Map: Brie Box")
        joiner = self.open_game()
        joiner.join(host.host())
        host.wait_for("s.players.length === 2")
        # The joiner runs about, jumping as they go.
        for key in ("w", "a", "s", "d"):
            joiner.page.keyboard.down(key)
            for _ in range(3):
                joiner.press("Space")
                joiner.page.wait_for_timeout(250)
            joiner.page.keyboard.up(key)
        # The host walks to the Gouda Launcher and fires it at the joiner.
        host.page.keyboard.down("w")
        host.wait_for("s.players.some((p) => p.mine && p.carrying.includes('launcher'))")
        host.page.keyboard.up("w")
        host.capture_mouse()
        host.hold_fire(1500)
        host.page.wait_for_timeout(1500)
        self.assertEqual(host.state()["put_backs"], 0)


if __name__ == "__main__":
    unittest.main()
