# Manual tests

Behavior that the automated tests (`python tools/run_tests.py`) don't cover and that has to be checked by a person. The automated tests cover how the project is put together: that it opens on the shooter level with the shooter's game mode, that the template's Blueprints load against our C++ classes, and that nothing any asset needs is missing.

**Keeping this current:** when a change adds or alters behavior that an automated test can't cover, add or update a row here in the same pull request. When a row becomes covered by an automated test, delete it.

**What to run:** the sections for whatever a change touches. Run everything before sharing a build with other people.

## The project

Setup: this PC, with the editor closed.

| ID | Test | Steps | Expected |
|---|---|---|---|
| PRJ-01 | It opens | 1. Open `CheeseStrike.uproject`. | The editor opens on the shooter level, after compiling shaders the first time, with no message about modules to rebuild or assets that failed to load. |
| PRJ-02 | It plays | 1. Press Play.<br>2. Walk, look, jump, pick up a weapon and fire it at the targets. | The first-person view moves with the keyboard and mouse, the weapon fires and hits, and the targets react. |
| PRJ-03 | Two players | 1. In the Play menu's options, set the players to 2 and the net mode to "Play As Listen Server".<br>2. Press Play and move in each window. | Each window sees the other player move, and neither is snapped back while moving. |
