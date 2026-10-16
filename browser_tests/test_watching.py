"""Watching a session rather than playing in it."""

import unittest

from harness import GameTestCase


class WatchingTest(GameTestCase):
    def test_a_player_joins_to_watch_and_then_decides_to_play(self):
        host = self.open_game()
        code = host.host()
        host_name = host.own_player()["name"]
        watcher = self.open_game()
        watcher.click("WatchButton")
        watcher.join(code)
        watcher_id = watcher.own_player()["id"]

        # There to watch: out of the arena, with nobody for the host to play.
        host.wait_for(f"s.players.some((p) => p.id === {watcher_id} && p.watching && !p.alive)")
        watcher.wait_for(f"s.combat.watch.startsWith('Watching {host_name}')")
        self.assertEqual(watcher.state()["combat"]["knocked_out"], "", "nobody knocked them out")

        # They change their mind, from the menu: in they come.
        watcher.open_menu()
        watcher.click("WatchToggleButton")
        for game in (host, watcher):
            game.wait_for(f"s.players.some((p) => p.id === {watcher_id} && !p.watching && p.alive)")
        watcher.wait_for("s.combat.watch === ''")

    def test_a_knocked_out_player_watches_until_they_are_back(self):
        host = self.open_game()
        joiner = self.open_game()
        joiner.join(host.host())
        joiner_id = joiner.own_player()["id"]
        host_name = host.own_player()["name"]
        host.wait_for("s.players.length === 2")
        host.page.wait_for_timeout(500)
        host.capture_mouse()
        host.shoot_until(f"s.players.some((p) => p.id === {joiner_id} && p.deaths >= 1)")
        # First how it happened, then the one who did it, until they are back.
        joiner.wait_for("s.replays_seen >= 1")
        self.assertEqual(host.state()["replays_seen"], 0, "only the one it happened to")
        joiner.wait_for(f"s.combat.watch === 'Watching {host_name}' || s.players.some((p) => p.mine && p.alive && p.deaths >= 1)")
        joiner.wait_for("s.players.some((p) => p.mine && p.alive && p.deaths >= 1) && s.combat.watch === '' && s.combat.replay === ''")


if __name__ == "__main__":
    unittest.main()
