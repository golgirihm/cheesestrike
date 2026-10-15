"""How much a player downloads before the game starts, held to a budget so
that the web build doesn't grow without anyone noticing."""

import unittest

from harness import WEB_BUILD

MB = 1024 * 1024

# The game's own data: arenas, models, sounds and scripts. Almost everything
# is made in code, so this is small; a budget well above what it is now
# leaves room to grow while catching something large added by mistake.
PCK_BUDGET = 4 * MB
# The page and the script that starts the engine.
PAGE_BUDGET = 1 * MB
# The engine itself. These tests use a debug export, which is larger than
# a release one; the budget is for that.
ENGINE_BUDGET = 48 * MB


class BuildSizeTest(unittest.TestCase):
    def size(self, name):
        path = WEB_BUILD / name
        self.assertTrue(path.exists(), f"{path} is missing; export the web build first")
        return path.stat().st_size

    def test_the_games_own_data_keeps_within_its_budget(self):
        self.assertLessEqual(self.size("index.pck"), PCK_BUDGET, "index.pck")

    def test_the_page_keeps_within_its_budget(self):
        self.assertLessEqual(self.size("index.html") + self.size("index.js"), PAGE_BUDGET)

    def test_the_engine_keeps_within_its_budget(self):
        self.assertLessEqual(self.size("index.wasm"), ENGINE_BUDGET, "index.wasm")


if __name__ == "__main__":
    unittest.main()
