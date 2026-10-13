"""Computer players: the host asks for them on the menu, and everyone sees
them play."""

import unittest

from harness import GameTestCase


class BotsTest(GameTestCase):
    def test_a_bot_joins_the_hosts_session_and_everyone_sees_it_move(self):
        host = self.open_game()
        host.choose("BotsButton", "1 bot")
        host.choose("BotSkillButton", "Bots: Easy")
        code = host.host()
        # Someone to play against, so the match begins at once.
        state = host.wait_for("s.players.length === 2 && !s.combat.match.startsWith('Warm-up')")
        bot = next(player for player in state["players"] if not player["mine"])
        self.assertLess(bot["id"], 0)
        self.assertTrue(bot["name"].startswith("[bot] "))
        moved = f"s.players.some((p) => p.id === {bot['id']} && Math.hypot(p.position[0] - ({bot['position'][0]}), p.position[2] - ({bot['position'][2]})) > 2)"
        host.wait_for(moved)

        joiner = self.open_game()
        joiner.join(code)
        seen = joiner.wait_for(f"s.players.length === 3 && s.players.some((p) => p.id === {bot['id']})")
        there = next(player for player in seen["players"] if player["id"] == bot["id"])
        joiner.wait_for(
            f"s.players.some((p) => p.id === {bot['id']} && (Math.hypot(p.position[0] - ({there['position'][0]}), p.position[2] - ({there['position'][2]})) > 2 || p.deaths > 0 || p.kills > 0))"
        )

    def test_a_bot_makes_way_when_the_session_is_full(self):
        host = self.open_game()
        host.choose("BotsButton", "1 bot")
        host.choose("MaxPlayersButton", "Up to 2 players")
        code = host.host()
        host.wait_for("s.players.length === 2 && s.players.some((p) => p.id < 0)")
        joiner = self.open_game()
        joiner.join(code)
        for game in (host, joiner):
            game.wait_for("s.players.length === 2 && s.players.every((p) => p.id > 0)")


if __name__ == "__main__":
    unittest.main()
