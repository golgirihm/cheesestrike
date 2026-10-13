"""Photo mode: the camera comes free, the HUD goes, and a picture is saved.

The game doesn't draw under test, so the picture saved here is of nothing:
what is checked is that one is made, the size of the view, and handed to the
browser to save. Having the game draw for this test took so much of the
machine, with no graphics card, that it is not worth it for one test. What a
picture looks like is a manual test."""

import struct
import unittest

from harness import GameTestCase


class PhotoTest(GameTestCase):
    def test_a_picture_is_saved(self):
        game = self.open_game()
        game.click("PracticeButton")
        state = game.wait_for("s.screen === 'game' && s.map === 'range' && s.players.length === 6")
        self.assertEqual(state["photo"], {"on": False, "saved": 0})
        self.assertNotEqual(state["combat"]["weapon"], "")
        self.assertTrue(state["weapon_bar"], "the weapons carried show, each with its key")

        game.press("p")
        state = game.wait_for("s.photo.on")
        self.assertEqual(state["combat"]["weapon"], "", "the HUD is out of the picture")
        self.assertEqual(state["hud"], "")

        with game.page.expect_download() as saving:
            game.press("Enter")
        picture = saving.value
        self.assertRegex(picture.suggested_filename, r"^cheesestrike-\d+\.png$")
        with open(picture.path(), "rb") as file:
            data = file.read()
        self.assertEqual(data[:8], b"\x89PNG\r\n\x1a\n", "it is a picture")
        width, height = struct.unpack(">II", data[16:24])
        self.assertGreater(width, 300, "the size of the view")
        self.assertGreater(height, 200)
        game.wait_for("s.photo.saved === 1")

        game.press("p")
        state = game.wait_for("!s.photo.on")
        self.assertNotEqual(state["combat"]["weapon"], "", "and the HUD comes back")
        self.assertEqual(state["photo"]["saved"], 1)


if __name__ == "__main__":
    unittest.main()
