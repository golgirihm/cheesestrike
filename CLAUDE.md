# CheeseStrike: Gouda Overload

A cheese-themed multiplayer first-person shooter. The game is a Godot 4.7 project in `game/`, played mainly in the browser, where one player's browser hosts and the others connect to it over WebRTC. `signaling/` is a small Python WebSocket server that lists sessions and brokers those connections. The README covers setup, running and building.

## Testing rules

Every change follows both of these, in the same pull request as the change:

1. **Automated tests.** Assess whether the change's behavior can be covered by an automated test. If it can, write the test with the change. Game tests are GUT tests in `game/test`; signaling server tests are in `signaling/test_server.py`; tests that play the web build in real browser windows are in `browser_tests/`, and suit anything a player does through the menu or across a connection. Logic that is tangled up with the network or the engine is worth pulling into a plain class so it can be tested, as `game/net/host_watch.gd` was.
2. **Manual tests.** Assess whether `docs/manual-tests.md` needs rows added, changed or removed. Behavior that an automated test can't cover gets a row. A row that becomes covered by an automated test is deleted.

Say what you concluded in the pull request description, including when the answer is that no test or no manual-test change was needed.

Run every suite before pushing:

```
python tools/run_tests.py
```

## Working in this repo

- **Pull requests only.** `main` is protected: changes arrive through a pull request, merged by rebase, with the "Game tests" and "Signaling tests" checks passing on an up-to-date branch. Keep each commit on a branch meaningful, since every one lands on `main`.
- **After merging, move on.** Once a pull request's checks have passed on an up-to-date branch, merge it and carry on; don't wait for the test run that the merge starts on `main`. That run tests the same code the pull request just tested. Watch it in the background instead and speak up only if it fails.
- **Delete the local branch after merging.** GitHub deletes the remote branch on merge; delete the local one too and return to an up-to-date `main`. A rebase merge rewrites the commits, so git won't see the branch as merged and `git branch -D` is needed.
- **No AI attribution.** Don't add "Generated with Claude Code" to pull request descriptions or a `Co-Authored-By` trailer for Claude to commits.
- **Godot on Windows.** Use `godot_console` in place of `godot` so output reaches the terminal.
- **Pinned versions.** CI's Python and Godot versions are in `tools/versions.env`; Python packages are pinned in the `requirements.txt` files in `signaling/` and `browser_tests/`. Change a version there, not in the workflow.
- **Web export.** Rebuild with `godot --headless --path game --export-debug Web ../build/web/index.html` and serve with `python tools/serve.py`, which turns caching off so a reload picks up the new build.
