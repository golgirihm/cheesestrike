"""Matches: the host's settings, the end of a match, teams, joining part way
through and the session's player limit."""

import unittest

from harness import GameTestCase

FIRST_TO_5 = "First to 5"


class MatchTest(GameTestCase):
    def host_with(self, mode="Free-for-all", kills="First to 20", players="Up to 16 players", map="Cheddar Canyon"):
        host = self.open_game()
        host.choose("ModeButton", mode)
        host.choose("KillLimitButton", kills)
        host.choose("MaxPlayersButton", players)
        host.choose("MapButton", f"Map: {map}")
        return host, host.host()

    def test_everyone_plays_on_the_map_the_host_chose(self):
        host, code = self.host_with(map="Brie Box")
        self.assertEqual(host.state()["map"], "box")
        joiner = self.open_game()
        self.assertEqual(joiner.state()["map"], "canyon", "the menu shows the default arena")
        joiner.join(code)
        joiner.wait_for("s.map === 'box'")
        # Standing on the small arena's floor, not fallen through it.
        joiner.page.wait_for_timeout(1000)
        self.assertGreater(joiner.own_player()["position"][1], -0.5)

    def test_a_match_waits_for_a_second_player_and_then_begins(self):
        host, code = self.host_with()
        self.assertEqual(host.state()["combat"]["match"], "Warm-up   ·   waiting for players")
        host.page.wait_for_timeout(1500)
        joiner = self.open_game()
        joiner.join(code)
        for game in (host, joiner):
            # The clock has only now started to run.
            game.wait_for("s.combat.match.startsWith('First to 20   ·   7:5')")
        host.wait_for("s.combat.announcement === 'The match begins'")

    def test_the_city_can_be_played_in(self):
        host, code = self.host_with(map="Curd City")
        self.assertEqual(host.state()["map"], "city")
        joiner = self.open_game()
        joiner.join(code)
        joiner.wait_for("s.map === 'city'")
        joiner_id = joiner.own_player()["id"]
        host.wait_for("s.players.length === 2")
        # Standing on the street, not fallen through it.
        joiner.page.wait_for_timeout(1000)
        self.assertGreater(joiner.own_player()["position"][1], -0.5)
        # The two start facing each other across the crossroads.
        host.capture_mouse()
        host.shoot_until(f"s.players.some((p) => p.id === {joiner_id} && (p.health < 100 || p.deaths > 0))")
        # Running straight ahead, the host reaches the launcher in the middle.
        host.page.keyboard.down("Shift")
        host.page.keyboard.down("w")
        try:
            host.wait_for("s.players.some((p) => p.mine && p.carrying.includes('launcher'))")
        finally:
            host.page.keyboard.up("w")
            host.page.keyboard.up("Shift")
        self.assertEqual(host.state()["put_backs"], 0, "running is allowed")

    def test_a_short_match_ends_with_a_winner_and_the_next_begins(self):
        # On the small arena, whoever is knocked out comes back straight ahead
        # of the host, the spot farthest from it.
        host, code = self.host_with(kills=FIRST_TO_5, map="Brie Box")
        joiner = self.open_game()
        joiner.join(code)
        joiner_id = joiner.own_player()["id"]
        host.wait_for("s.players.length === 2")
        host.page.wait_for_timeout(500)
        state = host.wait_for("s.combat.match.startsWith('First to 5')")
        self.assertRegex(state["combat"]["match"], r"First to 5   ·   \d:\d\d$")

        host.capture_mouse()
        own_name = host.own_player()["name"]
        host.shoot_until(f"s.combat.result === {own_name + ' wins!'!r}", timeout=180_000)
        # Everyone is shown the best moment of it again.
        for game in (host, joiner):
            game.wait_for(f"s.combat.replay.startsWith({'Play of the match: ' + own_name!r}) && s.combat.replay_ghosts >= 1")
        for game in (host, joiner):
            state = game.wait_for(f"s.combat.result === {own_name + ' wins!'!r}")
            self.assertEqual(state["combat"]["match"], "Match over")
        # The host, and only the host, is shown what each weapon did.
        tally = host.wait_for("s.combat.tally !== ''")["combat"]["tally"]
        self.assertRegex(tally, r"Cheddar Blaster: \d+ fired, \d+ hit, \d+ damage, 5 knocked out")
        self.assertEqual(joiner.state()["combat"]["tally"], "")
        self.assertEqual(host.player(joiner_id)["deaths"], 5)

        # After the break, a new match with the scores cleared.
        for game in (host, joiner):
            game.wait_for("s.combat.result === '' && s.combat.match.startsWith('First to 5') && s.players.every((p) => p.kills === 0 && p.deaths === 0)")

    def test_the_session_list_shows_the_hosts_settings_and_its_limit_holds(self):
        host, code = self.host_with(mode="Cheddar vs Brie", players="Up to 2 players")
        watcher = self.open_game()
        watcher.click("RefreshButton")
        watcher.wait_for(f"s.sessions.some((line) => line.text === '{code}  ·  1/2  ·  Cheddar vs Brie  ·  Cheddar Canyon')")

        joiner = self.open_game()
        joiner.join(code)
        host.wait_for("s.players.length === 2")
        late = self.open_game()
        late.click("TargetEdit")
        late.type(code)
        late.press("Enter")
        state = late.wait_for("s.status === 'That session is full.'")
        self.assertEqual(state["screen"], "menu")

    def test_teams_split_players_and_show_on_the_scoreboard(self):
        host, code = self.host_with(mode="Cheddar vs Brie")
        joiner = self.open_game()
        joiner.join(code)
        joiner_id = joiner.own_player()["id"]
        state = host.wait_for("s.players.length === 2 && s.players.every((p) => p.team !== null && p.team >= 0)")
        teams = {player["id"]: player["team"] for player in state["players"]}
        self.assertEqual(sorted(teams.values()), [0, 1], "one on each team")
        self.assertEqual(teams[1], 0, "the host starts on Cheddar")
        host.wait_for("s.combat.match.startsWith('Cheddar 0  –  0 Brie')")

        # Opponents can hurt each other.
        host.page.wait_for_timeout(500)
        host.capture_mouse()
        host.shoot_until(f"s.players.some((p) => p.id === {joiner_id} && p.deaths >= 1)")
        host.wait_for("s.combat.match.startsWith('Cheddar 1')")

        joiner.page.keyboard.down("Tab")
        board = joiner.wait_for("s.combat.scoreboard.length === 5")["combat"]["scoreboard"]
        joiner.page.keyboard.up("Tab")
        self.assertEqual(board[1][:2], ["Cheddar", "1"])
        self.assertEqual(board[3][:2], ["Brie", "0"])

    def test_someone_joining_part_way_through_sees_the_match_as_it_is(self):
        host, code = self.host_with()
        joiner = self.open_game()
        joiner.join(code)
        joiner_id = joiner.own_player()["id"]
        host.wait_for("s.players.length === 2")
        host.page.wait_for_timeout(500)
        host.capture_mouse()
        host.shoot_until(f"s.players.some((p) => p.id === {joiner_id} && p.deaths >= 1)")
        deaths = host.player(joiner_id)["deaths"]

        late = self.open_game()
        late.join(code)
        state = late.wait_for(f"s.players.length === 3 && s.players.some((p) => p.id === {joiner_id} && p.deaths === {deaths})")
        own = next(player for player in state["players"] if player["mine"])
        self.assertEqual(own["kills"], 0)
        self.assertEqual(own["health"], 100)
        self.assertEqual(next(p for p in state["players"] if p["id"] == 1)["kills"], deaths)
        late.wait_for("s.combat.match.startsWith('First to 20   ·   7:') || s.combat.match.startsWith('First to 20   ·   8:')")


if __name__ == "__main__":
    unittest.main()
