"""The arena editor: laying out an arena, hosting it for someone who has
never seen it, and passing it on in a link."""

import unittest

from harness import GameTestCase


class ArenaEditorTest(GameTestCase):
    def make_arena(self, game):
        """Lays one piece out in the editor and returns the link to the arena
        and its code."""
        game.click("EditorButton")
        state = game.wait_for("s.screen === 'editor' && s.map === 'custom'")
        self.assertIn("0 of 120 pieces", state["editor"][0])
        # The first click takes the mouse; the next places the piece shown
        # where the middle of the view meets the floor.
        game.capture_mouse()
        game.page.mouse.click(1, 1)
        state = game.wait_for("s.editor[0].includes('1 of 120 pieces')")
        link = state["editor_link"]
        self.assertIn("arena=", link)
        game.press("Escape")
        game.wait_for("!s.mouse_captured")
        game.click("EditorCopy")
        game.wait_for("s.editor.includes('Link copied')")
        game.click("EditorDone")
        state = game.wait_for("s.screen === 'menu' && s.custom_arena")
        self.assertEqual(link.split("arena=")[1], state["custom_arena"])
        self.assertNotEqual(state["map"], "custom")
        return link, state["custom_arena"]

    def test_an_arena_made_in_the_editor_is_hosted_for_whoever_joins(self):
        host = self.open_game(fresh=True)
        _, arena = self.make_arena(host)
        host.choose("MapButton", "Custom arena")
        code = host.host()
        state = host.wait_for("s.screen === 'game' && s.map === 'custom' && s.players.length === 1")
        self.assertEqual(state["arena_shown"], arena)
        # Someone who has made no arena of their own is sent the host's.
        guest = self.open_game(fresh=True)
        self.assertEqual(guest.state()["custom_arena"], "")
        guest.join(code)
        state = guest.wait_for("s.map === 'custom' && s.players.length === 2")
        self.assertEqual(state["arena_shown"], arena)
        self.assertEqual(state["custom_arena"], "", "and it isn't kept as theirs")

    def test_a_link_gives_the_arena_to_whoever_opens_it(self):
        maker = self.open_game(fresh=True)
        link, arena = self.make_arena(maker)
        other = self.open_link(link)
        state = other.wait_for("s.screen === 'menu' && s.custom_arena")
        self.assertEqual(state["custom_arena"], arena)
        self.assertEqual(state["match_buttons"][4], "Custom arena")
        self.assertIn("arena from the link", state["status"])
        self.assertNotIn("arena=", state["address"], "taken off the address once read")
        # It opens in their editor as it was made.
        other.click("EditorButton")
        state = other.wait_for("s.screen === 'editor'")
        self.assertIn("1 of 120 pieces", state["editor"][0])

    def test_with_no_arena_made_the_usual_one_is_hosted(self):
        game = self.open_game(fresh=True)
        game.choose("MapButton", "Custom arena (none made yet)")
        game.host()
        state = game.wait_for("s.screen === 'game' && s.players.length === 1")
        self.assertEqual(state["map"], "canyon")


if __name__ == "__main__":
    unittest.main()
