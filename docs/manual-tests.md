# Manual tests

Behavior that the automated tests (`python tools/run_tests.py`) don't cover and that has to be checked by a person. The automated tests cover how the project is put together: that it opens on the shooter level with the shooter's game mode, that the template's Blueprints load against our C++ classes, and that nothing any asset needs is missing.

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

Setup: Play in Editor on the shooter level, alone and then as a listen server with two players (PRJ-03). Why manual: how firing feels, and whether the server agrees with what the shooter saw.

| ID | Test | Steps | Expected |
|---|---|---|---|
| SHT-01 | Firing | 1. Click once; hold the button for a second; let go and hold again.<br>2. Fire at a wall close by, then across the arena. | One click fires one shot. Holding fires about eight a second, the rifle kicking with each, until the magazine is empty, when it reloads by itself. The count at the lower right falls one a shot and reads "RELOADING" for a second and a half, then 24 with the reserve lower. Each shot lands where the crosshair was, close or far. |
| SHT-02 | Spread, recoil and aiming | 1. Hold the trigger for the whole magazine at a wall and watch the crosshair and the view.<br>2. Hold the right button, then fire a burst. | From the hip the crosshair opens over the first eight shots and the view climbs steadily, less with each shot, settling after about nine, and wanders a little from side to side; the shots scatter more as the crosshair opens. Aiming narrows the view to six tenths, the crosshair nearly closes, and a burst climbs less and scatters far less. Letting go of the trigger for a third of a second starts the pattern over. |
| SHT-03 | Hitting another player | 1. As a listen server with two players, have B stand still and A fire at B's body, then B's head, from five metres and from thirty.<br>2. Have B sprint across A's view while A fires. | Each hit shows A four diagonal ticks by the crosshair, longer and yellow for the head, and B's health at the lower left falls by 20 a body shot close up, 35 on the head, and less at thirty metres. Shots at the running B that were on them when A fired count, within a fraction of a second of lag. The server's log names each hit and the health left. |
| SHT-04 | Reloading | 1. Fire a few shots and press R; press R again at once; press R with a full magazine. | R reloads when the magazine is short, taking a second and a half, and fires nothing meanwhile; a second R does nothing, and nor does R with a full magazine. The reserve goes down by what the magazine took. |

## Movement

Setup: Play in Editor on the shooter level, with the output log open to watch for corrections. Why manual: how it feels, and whether the server agrees with what the player saw.

| ID | Test | Steps | Expected |
|---|---|---|---|
| MOV-01 | Walk, sprint, crouch | 1. Walk, then hold Shift and walk.<br>2. Hold C (or Ctrl) and walk; let go.<br>3. Hold Shift and C together from standing. | Sprinting is plainly faster than walking, about half as fast again. Crouching lowers the view by about a head's height in a fraction of a second and halves the pace. Crouched, Shift does nothing. |
| MOV-02 | Jump and air steering | 1. Jump from standing, then from a sprint.<br>2. Jump and let go of every key; jump and steer sideways in the air.<br>3. Walk into a knee-high step, then into a waist-high block. | A jump is quick and about half a metre high; from a sprint it carries forward. Letting go in the air slows the character gently, not at once; steering in the air turns the landing a little, not sharply. A low step is walked up; a waist-high block stops the character. |
| MOV-03 | Slide | 1. Sprint, then press and hold C.<br>2. Try the same from a walk.<br>3. Sprint, slide, stand, and slide again at once.<br>4. During a slide, push sideways, and look round. | From a sprint, crouching slides: the character keeps going the way it was, a touch faster at first, then eases to a crouched walk over about a second, and stays crouched while C is held. From a walk, C only crouches. Right after a slide the next needs a moment. Pushing sideways during a slide does nothing; looking round does not change the slide's direction. |
| MOV-05 | Hands and weapon | 1. Look level, up, down and round while standing, walking and sprinting.<br>2. Crouch and look down. | The rifle is in view at the lower right, steady as the view turns and bobbing a little with the walk. Crouched, it stays in view. (The template's arms don't yet hold it as its own character's do; that waits for the game's own arms.) |
| MOV-04 | Nobody is corrected | 1. Play as a listen server with two players (PRJ-03). In the joining window, sprint, slide, crouch and jump about for a minute.<br>2. Watch the host's view of the joiner, and the output log. | The joiner is never snapped back or jerked, in its own window or the host's, and the log has no line about a client adjustment or a position correction. |
