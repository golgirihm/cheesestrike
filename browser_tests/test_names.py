"""Players' names: chosen in the menu, cleaned by the host, shown to everyone
else, and remembered between visits."""

import unittest

from harness import GameTestCase


class NamesTest(GameTestCase):
    def set_name(self, game, name):
        # Clicking the box selects what is in it, so typing replaces it.
        game.click("NameEdit")
        game.type(name)
        game.press("Enter")
        return game.wait_for(f"s.name_box === {name.strip()!r}")

    def test_each_player_sees_the_others_by_name(self):
        host = self.open_game()
        self.set_name(host, "Host Havarti")
        code = host.host()
        joiner = self.open_game()
        self.set_name(joiner, "Jarlsberg Joe")
        joiner.join(code)

        for game, other in ((host, "Jarlsberg Joe"), (joiner, "Host Havarti")):
            state = game.wait_for(f"s.players.some((p) => !p.mine && p.name === {other!r})")
            mine = next(player for player in state["players"] if player["mine"])
            self.assertNotEqual(mine["name"], other)

    def test_the_host_cleans_a_name_before_showing_it(self):
        host = self.open_game()
        code = host.host()
        joiner = self.open_game()
        joiner.click("NameEdit")
        joiner.type("  Blue    Cheese ")
        joiner.join(code)
        host.wait_for("s.players.some((p) => !p.mine && p.name === 'Blue Cheese')")

    def test_a_blank_name_keeps_the_last_one(self):
        game = self.open_game()
        self.set_name(game, "Comte")
        game.click("NameEdit")
        game.type("   ")
        game.press("Enter")
        game.wait_for("s.name_box === 'Comte'")

    def test_the_name_is_remembered_after_a_reload(self):
        game = self.open_game()
        self.set_name(game, "Saved Raclette")
        # The browser stores the file a moment after the game writes it.
        game.page.wait_for_timeout(1500)
        game.page.reload()
        game.wait_for("s.screen === 'menu' && s.name_box === 'Saved Raclette'")


if __name__ == "__main__":
    unittest.main()
