# CheeseStrike: Gouda Overload

A cheese-themed multiplayer first-person shooter. The game is a Godot 4.7 project in `game/`, played mainly in the browser, where one player's browser hosts and the others connect to it over WebRTC. `signaling/` is the small Python server that lists sessions and brokers those connections: one set of rules (`signaling/core.py`), run by a single process in development (`signaling/local/`) and by a Lambda function on AWS (`signaling/cloud/`), which `infra/` sets up and deploys. The README covers setup, running and building; `docs/backend.md` covers the backend.

## Testing rules

Every change follows both of these, in the same pull request as the change:

1. **Automated tests.** Assess whether the change's behavior can be covered by an automated test. If it can, write the test with the change. Game tests are GUT tests in `game/test`; signaling tests are in `signaling/` (a rule belongs in `test_stores.py`, which runs it over both the in-memory and the DynamoDB store); tests of the AWS template and the deploy script are in `infra/`; tests that play the web build in real browser windows are in `browser_tests/`, and suit anything a player does through the menu or across a connection. Logic that is tangled up with the network or the engine is worth pulling into a plain class so it can be tested, as `game/net/host_watch.gd` was.
2. **Manual tests.** Assess whether `docs/manual-tests.md` needs rows added, changed or removed. Behavior that an automated test can't cover gets a row. A row that becomes covered by an automated test is deleted.

Say what you concluded in the pull request description, including when the answer is that no test or no manual-test change was needed.

Before pushing, run the tests for what you changed: the GUT files it touches, or a suite by name, such as `python tools/run_tests.py game`. CI runs every suite on the pull request, browser tests included, and a pull request is merged only once all of its checks have passed. Run everything locally (`python tools/run_tests.py`) only when a failure in CI needs reproducing.

## Phones are deprecated

The game is played in a desktop browser, with keyboard and mouse or a gamepad. Nobody is expected to play it on a phone, so the phone experience (touch controls, gyro aim, the installable web app, full screen and sideways on a tap) stays as it is, with its tests, but gets no further work:

- Don't add features, fixes or settings only for phones, and don't extend the touch layout to cover new actions.
- Don't hold anything back for phones: not the picture, the frame budget, the build size, the interface's layout, nor what a host can simulate. The browser and its Compatibility renderer are the limits, not a phone.
- If a change breaks something that only matters on a phone and fixing it isn't easy, let it break and delete the tests and manual-test rows that cover it.

## Working in this repo

- **Pull requests only.** `main` is protected: changes arrive through a pull request, merged by rebase, with the "Game tests" and "Signaling tests" checks passing on an up-to-date branch. Keep each commit on a branch meaningful, since every one lands on `main`.
- **After merging, move on.** Once a pull request's checks have passed on an up-to-date branch, merge it and carry on; don't wait for the test run that the merge starts on `main`. That run tests the same code the pull request just tested. Watch it in the background instead and speak up only if it fails.
- **Delete the local branch after merging.** GitHub deletes the remote branch on merge; delete the local one too and return to an up-to-date `main`. A rebase merge rewrites the commits, so git won't see the branch as merged and `git branch -D` is needed.
- **No AI attribution.** Don't add "Generated with Claude Code" to pull request descriptions or a `Co-Authored-By` trailer for Claude to commits.
- **Godot on Windows.** Use `godot_console` in place of `godot` so output reaches the terminal.
- **Pinned versions.** CI's Python and Godot versions are in `tools/versions.env`; Python packages are pinned in the `requirements` files in `signaling/` and `browser_tests/`. Change a version there, not in the workflow.
- **Licensed assets.** Bought or downloaded models, textures and animations go in `game/licensed/`, a submodule from the private `cheesestrike-assets` repository, never in this public one, because their licenses forbid sharing the files. Add a row to its `LICENSES.md` for each, with the source and license. Only game-ready files go there; the original downloads stay outside git. A new worktree needs `git submodule update --init`. CI fetches the submodule only when it has the deploy key, so a fork's pull request and the buildci command run without it: the game and its tests must still work when `game/licensed/` is empty. The web build leaves it out altogether, so the browser tests always play without it.
- **Web export.** Rebuild with `godot --headless --path game --export-debug Web ../build/web/index.html` and serve with `python tools/serve.py`, which turns caching off so a reload picks up the new build.
