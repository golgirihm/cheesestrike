# Manual tests

Behavior that the automated tests (`python tools/run_tests.py`) don't cover and that has to be checked by a person. The automated tests cover how the project is put together (it opens on the shooter level with the game's own mode, the template's Blueprints load against our C++ classes, nothing any asset needs is missing), the rules as plain functions (movement numbers, weapon damage and spread, where the killed come back, the scoreboard's order, the kill limit), and, playing the game in the editor (`CheeseStrike.Play.*`), that the character walks, sprints, slides, crouches and jumps at the right speeds, that the weapon fires, aims, empties and reloads by its numbers, and that over the network a kill is scored and shown, the killed come back at a start with full health and protection, and the match ends at the kill limit with a winner.

**Keeping this current:** when a change adds or alters behavior that an automated test can't cover, add or update a row here in the same pull request. When a row becomes covered by an automated test, delete it.

**What to run:** the sections for whatever a change touches. Run everything before sharing a build with other people.

## The project

Setup: this PC, with the editor closed.

| ID | Test | Steps | Expected |
|---|---|---|---|
| PRJ-01 | It opens | 1. Open `CheeseStrike.uproject`. | The editor opens on the shooter level, after compiling shaders the first time, with no message about modules to rebuild or assets that failed to load. |
| PRJ-02 | It plays | 1. Press Play.<br>2. Walk and look with the keyboard and mouse, then with a gamepad. | The first-person view moves with W, A, S, D and the arrows, looks with the mouse and the right stick, and looks up when the mouse or stick goes up. The mannequin's arms are seen below the view. The template's weapons on the floor can't be picked up yet. |
| PRJ-03 | Two players | 1. Click the three-dot button beside Play, and under Multiplayer Options set Number of Players to 2 and Net Mode to "Play As Listen Server".<br>2. Press Play and move, sprint, crouch and jump in each window while watching the other. | Each window sees the other player move, crouch and jump where and when they did, and neither is snapped back while moving. The joining window's own movement feels as immediate as the host's. |

## Shooting

Setup: Play in Editor on the shooter level, alone and then as a listen server with two players (PRJ-03). Why manual: how firing feels and looks, and whether the server agrees with what the shooter saw of a moving target.

| ID | Test | Steps | Expected |
|---|---|---|---|
| SHT-01 | Firing | 1. Click once; hold the button for a second; let go and hold again.<br>2. Fire at a wall close by, then across the arena. | One click fires one shot. Holding fires about eight a second, the rifle kicking with each, until the magazine is empty, when it reloads by itself. The count at the lower right falls one a shot and reads "RELOADING" while it reloads. Each shot lands where the crosshair was, close or far. |
| SHT-02 | Spread, recoil and aiming | 1. Hold the trigger for the whole magazine at a wall and watch the crosshair and the view.<br>2. Hold the right button, then fire a burst. | From the hip the crosshair opens over the first eight shots and the view climbs steadily, less with each shot, settling after about nine, and wanders a little from side to side; the shots scatter more as the crosshair opens. Aiming narrows the view, the crosshair nearly closes, and a burst climbs less and scatters far less. Letting go of the trigger for a third of a second starts the pattern over. |
| SHT-03 | Hitting a moving player | 1. As a listen server with two players, have B stand still and A fire at B's head from five metres.<br>2. Have B sprint across A's view while A fires. | A head hit shows A longer, yellow ticks by the crosshair; a body hit shorter white ones. Shots at the running B that were on them when A fired count, within a fraction of a second of lag. The server's log names each hit and the health left. |

## Kills

Setup: Play in Editor as a listen server with two players (PRJ-03). Why manual: how dying, coming back and the match's end look and feel.

| ID | Test | Steps | Expected |
|---|---|---|---|
| KIL-01 | Dying | 1. Have A kill B.<br>2. In B's window, look round and try to move and fire. | B's body falls where it stood and lies there, seen from where B's eyes were; B can look round but not move or fire. B's window says who killed them and counts down three seconds; A's window shows the kill at the top right for a few seconds, and A's crosshair shows the hit marks. |
| KIL-02 | Coming back | 1. After the three seconds, in B's window, move and fire.<br>2. Have A shoot B at once, then again after three seconds. | B is back at a player start, as far from A as the level allows, with full health, "PROTECTED" by the health, and the rifle full. A's shots in the first three seconds do nothing to B and show A no hit mark; after that, or as soon as B fires, they hurt. The fallen body is gone a few seconds after B is back. |
| KIL-03 | The scoreboard | 1. Hold Tab in each window, during play and after a kill. | A panel in the middle names the match, "first to N kills", and lists both players with their kills and deaths, the leader first, while Tab is held, and goes when it is let go. |
| KIL-04 | The match ends | 1. Host with `?KillLimit=2` on the map's URL (in the Play menu's options, or from the command line), and have A score two kills. | At the second kill the scoreboard stays up in both windows, headed "<A> WINS", and nobody can move, fire or come back. |

## Movement

Setup: Play in Editor on the shooter level, with the output log open to watch for corrections. Why manual: how it feels and looks, and whether the server agrees with what the player saw.

| ID | Test | Steps | Expected |
|---|---|---|---|
| MOV-02 | Jump and air steering | 1. Jump from standing, then from a sprint.<br>2. Jump and let go of every key; jump and steer sideways in the air.<br>3. Walk into a knee-high step, then into a waist-high block. | A jump is quick and about half a metre high; from a sprint it carries forward. Letting go in the air slows the character gently, not at once; steering in the air turns the landing a little, not sharply. A low step is walked up; a waist-high block stops the character. |
| MOV-03 | Slide | 1. Sprint, slide, stand, and slide again at once.<br>2. During a slide, push sideways, and look round. | Right after a slide the next needs a moment. Pushing sideways during a slide does nothing; looking round does not change the slide's direction. The slide looks like one: a dip of the view that comes back up as it ends. |
| MOV-05 | Hands and weapon | 1. Look level, up, down and round while standing, walking and sprinting.<br>2. Crouch and look down. | The rifle is in view at the lower right, steady as the view turns and bobbing a little with the walk. Crouched, it stays in view. (The template's arms don't yet hold it as its own character's do; that waits for the game's own arms.) |
| MOV-04 | Nobody is corrected | 1. Play as a listen server with two players (PRJ-03). In the joining window, sprint, slide, crouch and jump about for a minute.<br>2. Watch the host's view of the joiner, and the output log. | The joiner is never snapped back or jerked, in its own window or the host's, and the log has no line about a client adjustment or a position correction. |
