"""Who you are: a title chosen and a level earned are kept between visits
and shown to the others in a session."""

import unittest

from harness import GameTestCase


class ProfileTest(GameTestCase):
    def test_the_profile_page_opens_from_the_title_and_comes_back(self):
        game = self.open_game(fresh=True)
        name = game.state()["name_box"]
        game.click("ProfileButton")
        state = game.wait_for("s.screen === 'profile'")
        self.assertEqual(state["profile"]["name"], name)
        self.assertEqual(state["profile"]["level"], "Level 1   ·   0 of 300 to the next")
        self.assertIn("Matches played: 0", state["profile"]["lines"][0])
        game._click("Profile_matches")
        state = game.wait_for("s.profile.part === 'matches'")
        self.assertEqual(state["profile"]["lines"], ["No matches yet. Play one, and it will be here."])
        game._click("ProfileDone")
        game.wait_for("s.screen === 'title'")

    def test_a_title_is_kept_between_visits_and_shown_to_others(self):
        host = self.open_game()
        code = host.host()
        joiner = self.open_game(fresh=True)
        joiner.click("CustomiseButton")
        joiner.wait_for("s.screen === 'settings'")
        joiner.scroll_to("Setting_look_title")
        joiner.choose_next("Setting_look_title")
        joiner.click("SettingsDone")
        # Their own page says so.
        joiner.click("ProfileButton")
        state = joiner.wait_for("s.screen === 'profile'")
        self.assertEqual(state["profile"]["rank"], "Fresh from the Dairy")
        joiner._click("ProfileDone")
        # The browser stores the file a moment after the game writes it.
        joiner.page.wait_for_timeout(1500)
        joiner.page.reload()
        joiner.to_play()
        joiner.join(code)
        joiner_id = joiner.own_player()["id"]
        state = host.wait_for(f"s.players.some((p) => p.id === {joiner_id} && p.look.title === 'fresh')")
        seen = next(player for player in state["players"] if player["id"] == joiner_id)
        self.assertEqual(seen["look"]["level"], 1)


if __name__ == "__main__":
    unittest.main()
