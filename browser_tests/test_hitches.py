"""No hitches: the first shot, pickup and burst of a session don't make the
game catch. This is the one test that has the game draw, since what it
guards against is the work of drawing something for the first time.

A browser on a machine with no graphics card does little of that work, so
this catches a frame that is grossly long, from whatever cause, rather than
proving the first draw is smooth on a player's machine: docs/manual-tests.md
has a row for that."""

import unittest

from harness import GameTestCase


class HitchesTest(GameTestCase):
    def longest_since(self, game, second):
        """The longest frame, in milliseconds, in the seconds after `second`
        that are over."""
        frames = game.state()["frames"]
        return max((ms for at, ms in frames["seconds"] if second < at < frames["now"]), default=0.0)

    def test_the_first_fight_has_no_frame_much_longer_than_the_rest(self):
        game = self.open_game("&draw")
        game.host()
        game.wait_for("s.screen === 'game' && s.players.length === 1")
        game.capture_mouse()
        # Let the session settle, and see what a frame takes with nothing new.
        game.page.wait_for_timeout(3000)
        before = game.state()["frames"]
        usual = before["typical"]
        self.assertGreater(usual, 0.0)

        # The first of each: a shot, a weapon picked up, a wheel launched and
        # burst.
        game.hold_fire(300)
        game.page.keyboard.down("w")
        try:
            game.wait_for("s.players.some((p) => p.mine && p.carrying.includes('launcher'))")
        finally:
            game.page.keyboard.up("w")
        game.wait_for("s.combat.weapon === 'Gouda Launcher'")
        loaded = int(game.state()["combat"]["ammo"].split(" / ")[0])
        game.hold_fire(100)
        # Fired, and then long enough for it to have flown and burst: the
        # wheel itself may be gone again between two looks at the game.
        game.wait_for(f"parseInt(s.combat.ammo) < {loaded}")
        game.page.wait_for_timeout(2500)
        self.assertEqual(game.state()["projectiles"], 0)
        # Until the seconds the fight was in are over and counted.
        game.wait_for(f"s.frames.now > {game.state()['frames']['now'] + 1}")

        longest = self.longest_since(game, before["now"])
        self.assertGreater(longest, 0.0)
        # A frame may be a few times the usual, as when the picture changes
        # a lot at once, but not the tenths of a second a first draw took.
        allowed = usual * 4.0 + 60.0
        self.assertLess(longest, allowed, f"a frame of {longest:.0f} ms, where {usual:.0f} ms is usual")


if __name__ == "__main__":
    unittest.main()
