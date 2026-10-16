"""Matches: the host's settings, the end of a match, teams, joining part way
through and the session's player limit."""

import unittest

from harness import GameTestCase

FIRST_TO_5 = "First to 5"


class MatchTest(GameTestCase):
    def host_with(self, mode="Free-for-all", kills="First to 20", players="Up to 16", map="Cheddar Canyon"):
        host = self.open_game()
        host.choose("ModeButton", mode)
        host.choose("KillLimitButton", kills)
        host.choose("MaxPlayersButton", players)
        host.choose("MapButton", f"{map}")
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

    def test_a_match_counts_down_while_nobody_moves_and_then_begins(self):
        host = self.open_game("&countdown")
        host.host()
        state = host.wait_for("s.combat.match.startsWith('The match begins in')")
        start = host.own_player()["position"]
        host.page.keyboard.down("w")
        host.page.wait_for_timeout(1000)
        self.assertAlmostEqual(host.own_player()["position"][2], start[2], delta=0.05, msg="held still")
        # Then the clock starts from the full time, and everyone can move.
        host.wait_for("s.combat.match.startsWith('First to 20   ·   ')")
        host.wait_for("s.combat.announcement === 'The match begins'")
        host.wait_for(f"Math.hypot(s.players.find((p) => p.mine).position[0] - ({start[0]}), s.players.find((p) => p.mine).position[2] - ({start[2]})) > 1")
        host.page.keyboard.up("w")

    def test_everyone_plays_by_the_hosts_rules(self):
        host = self.open_game()
        host.choose("RulesButton", "Instant knock-out")
        code = host.host()
        joiner = self.open_game()
        joiner.join(code)
        # Said as the second player arrives, and only for a few seconds: looked
        # for first, before anything that can take a slow machine a while.
        host.wait_for("s.combat.announcement === 'The match begins  ·  Instant knock-out'")
        joiner_id = joiner.own_player()["id"]
        for game in (host, joiner):
            game.wait_for("s.players.length === 2 && s.combat.health === '1'")
        host.page.wait_for_timeout(500)
        host.capture_mouse()
        state = host.shoot_until(f"s.players.some((p) => p.id === {joiner_id} && p.deaths >= 1)")
        self.assertGreaterEqual(next(player for player in state["players"] if player["mine"])["ammo"], 14, "it didn't take many shots")

    def test_a_player_opens_and_shuts_a_door_with_e_and_everyone_sees_it(self):
        # In the dairy the third player to arrive starts inside the cold
        # store, facing its door, with the middle of the arena beyond.
        host, code = self.host_with(map="Double Cream Dairy")
        joiner = self.open_game()
        joiner.join(code)
        host.wait_for("s.players.length === 2")
        inside = self.open_game()
        inside.join(code)
        for game in (host, joiner, inside):
            state = game.wait_for("s.players.length === 3 && s.map === 'dairy' && s.doors.length === 1")
            self.assertEqual(state["doors"], [0], "shut, though someone is beside it")
            self.assertEqual(state["doors_shown"], [0])
        inside.press("e")
        for game in (host, joiner, inside):
            game.wait_for("s.doors[0] !== 0 && s.doors_shown[0] === 1")
        # Out through the doorway; it stays open behind them until shut.
        inside.page.keyboard.down("w")
        try:
            inside.wait_for("s.players.some((p) => p.mine && p.position[0] > -10)")
        finally:
            inside.page.keyboard.up("w")
        inside.page.wait_for_timeout(1000)
        self.assertNotEqual(host.state()["doors"], [0], "left open")
        inside.press("e")
        for game in (host, joiner, inside):
            game.wait_for("s.doors[0] === 0 && s.doors_shown[0] === 0")

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
        # The city is large: the first two start at either end of it.
        start = host.own_player()["position"]
        far = joiner.own_player()["position"]
        self.assertGreater(abs(start[0] - far[0]), 500, "a long way apart")
        self.assertNotEqual(joiner_id, host.own_player()["id"])
        # Running straight ahead, toward the middle, the host covers ground
        # and is never put back for it.
        host.capture_mouse()
        host.page.keyboard.down("Shift")
        host.page.keyboard.down("w")
        try:
            host.wait_for(f"s.players.some((p) => p.mine && Math.abs(p.position[0]) < {abs(start[0]) - 20})")
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
        # Each is shown who did best, and what they themselves did.
        joiner_name = joiner.own_player()["name"]
        for game in (host, joiner):
            summary = game.wait_for("s.combat.summary.length >= 3 && s.combat.summary[2].includes('Damage')")["combat"]["summary"]
            self.assertEqual(summary[0], f"1st   {own_name}   ·   5")
            self.assertEqual(summary[1], f"2nd   {joiner_name}   ·   0")
        won = host.state()["combat"]["summary"]
        self.assertRegex(won[2], r"^Accuracy \d+%   ·   Damage \d+   ·   Best run 5   ·   Favourite: Cheddar Blaster$")
        self.assertTrue(won[3].startswith("Medals: "), won)
        self.assertIn("On a run", won[3], "five without being knocked out")
        self.assertGreaterEqual(int(won[2].split("Damage ")[1].split(" ")[0]), 500)
        lost = joiner.state()["combat"]["summary"]
        self.assertRegex(lost[2], r"^Accuracy 0%   ·   Damage 0   ·   Best run 0$")
        self.assertEqual(len(lost), 4, "no medals for the one who lost")
        # And what it earned them, which is kept: experience, and for the
        # winner a level, their first feats and what those and the level gave.
        earned = next(line for line in won if line.startswith("+"))
        self.assertRegex(earned, r"^\+\d+ experience   ·   Level \d+ reached!$")
        self.assertIn("Achievement: First Crumb", won)
        self.assertIn("Achievement: Top of the Board", won)
        self.assertTrue(any(line.startswith("Unlocked   ·   ") for line in won), won)
        self.assertRegex(lost[-1], r"^\+\d+ experience   ·   Level 1$")
        for game in (host, joiner):
            progress = game.state()["progress"]
            self.assertEqual(progress["matches"], 1)
            self.assertEqual(progress["history"], 1)
            self.assertGreater(progress["xp"], 0)
        self.assertGreater(host.state()["progress"]["xp"], joiner.state()["progress"]["xp"], "more for the one who won")
        # The host, and only the host, is shown what each weapon did.
        tally = host.wait_for("s.combat.tally !== ''")["combat"]["tally"]
        self.assertRegex(tally, r"Cheddar Blaster: \d+ fired, \d+ hit, \d+ damage, 5 knocked out")
        self.assertEqual(joiner.state()["combat"]["tally"], "")
        self.assertEqual(host.player(joiner_id)["deaths"], 5)

        # After the break, a new match with the scores cleared.
        for game in (host, joiner):
            game.wait_for("s.combat.result === '' && s.combat.match.startsWith('First to 5') && s.players.every((p) => p.kills === 0 && p.deaths === 0)")

    def test_the_session_list_shows_the_hosts_settings_and_its_limit_holds(self):
        host, code = self.host_with(mode="Cheddar vs Brie", players="Up to 2")
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
