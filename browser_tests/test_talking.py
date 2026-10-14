"""Talking: a player holds the Talk key and another hears them, over the
connections the game already has. The browser is given a made-up microphone,
which sounds a tone, and allows its use without asking."""

import unittest

from harness import GameTestCase


class TalkingTest(GameTestCase):
    def talk(self, game, milliseconds):
        game.page.keyboard.down("k")
        game.page.wait_for_timeout(milliseconds)
        game.page.keyboard.up("k")

    def test_one_player_talks_and_the_other_hears_until_they_mute_them(self):
        host = self.open_game()
        code = host.host()
        talker = self.open_game(fresh=True)
        talker.join(code)
        talker_id = talker.own_player()["id"]
        for game in (host, talker):
            game.wait_for("s.players.length === 2")
        self.assertEqual(host.state()["voice"], {"sent": 0, "heard": 0, "talking": [], "muted": [], "unheard": []})

        # The microphone starts on the first press, so hold it a while.
        talker.page.keyboard.down("k")
        try:
            talker.wait_for("s.voice.sent > 0")
            state = host.wait_for(f"s.voice.heard > 0 && s.voice.talking.includes({talker_id})")
            self.assertIn("Talking: ", state["hud"])
        finally:
            talker.page.keyboard.up("k")
        # Let go, they stop being heard, and nobody hears themselves.
        host.wait_for("s.voice.talking.length === 0")
        self.assertEqual(talker.state()["voice"]["heard"], 0)

        # Muted, nothing more of theirs is heard.
        host.click("MenuButton")
        host.click("MuteButton")
        host.wait_for(f"s.voice.muted.includes({talker_id})")
        host.click("ResumeButton")
        heard = host.state()["voice"]["heard"]
        sent = talker.state()["voice"]["sent"]
        talker.page.keyboard.down("k")
        try:
            talker.wait_for(f"s.voice.sent > {sent + 3}")
        finally:
            talker.page.keyboard.up("k")
        host.page.wait_for_timeout(500)
        self.assertEqual(host.state()["voice"]["heard"], heard)
        self.assertEqual(host.state()["voice"]["talking"], [])


    def test_a_blocked_player_stays_unheard_when_they_come_back(self):
        host = self.open_game()
        code = host.host()
        other = self.open_game(fresh=True)
        other.join(code)
        first_id = other.own_player()["id"]
        host.wait_for("s.players.length === 2")
        host.click("MenuButton")
        # Mute, then the same button again to block them for good.
        host.click("MuteButton")
        host.wait_for(f"s.voice.muted.includes({first_id})")
        host.click("MuteButton")
        host.page.wait_for_timeout(300)
        host.click("ResumeButton")

        # They leave and join again, as someone new to the session.
        other.page.reload()
        other.wait_for("s.screen === 'menu' || s.screen === 'title'")
        other.join(code)
        second_id = other.own_player()["id"]
        self.assertNotEqual(second_id, first_id)
        host.wait_for(f"s.players.length === 2 && s.voice.unheard.includes({second_id})")


if __name__ == "__main__":
    unittest.main()
