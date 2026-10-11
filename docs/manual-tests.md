# Manual tests

Behavior that the automated tests don't cover and that has to be checked by a person. The automated suites (`python tools/run_tests.py`) cover the rules for detecting a lost host, the signaling server's protocol and, by playing the game in headless browser windows, hosting, finding, joining and leaving sessions, a late joiner seeing everyone in place, being told when the host closes the page, players' names, shooting (hits, damage, knock-outs, respawning and reloading), matches (settings in the session list, the player limit, winning, teams, joining part way through) and the in-game menu. What a player sees and feels, input devices, real phones and other browsers are here.

More of these could move to the browser tests in `browser_tests/`. A row is here either because it needs a person or real hardware, or because nobody has automated it yet.

**Keeping this current:** when a change adds or alters behavior that an automated test can't cover, add or update a row here in the same pull request. When a row becomes covered by an automated test, delete it.

**What to run:** the sections for whatever a change touches. Run everything before sharing a build with other people.

## Setups

Each section names the setup it needs. Build and serve the web version first, as described in the [README](../README.md#development).

| Setup | What it is |
|---|---|
| **Two windows** | Two browser windows side by side on one PC, both at `http://localhost:8060`. One hosts, the other joins. Both stay visible unless a step says otherwise. |
| **Phone and PC** | A phone on the same Wi-Fi as the PC, at `http://<PC address>:8060`, and one browser window on the PC. |
| **Native pair** | Two native instances on one PC: `godot --path game -- --host` and `godot --path game -- --join=127.0.0.1`. |

## Movement and view

Setup: two windows; do the steps in either one. Why manual: this is how the controls feel and what the screen shows.

| ID | Test | Steps | Expected |
|---|---|---|---|
| MOV-01 | Walk | 1. Click in the game.<br>2. Hold W, A, S, D in turn, then the arrow keys. | The character moves forward, left, back and right relative to where it is facing, at a steady speed, and stops when the key is released. |
| MOV-02 | Look with the mouse | 1. Click in the game.<br>2. Move the mouse in all directions.<br>3. Press Esc. | After the click the pointer disappears and the view follows the mouse. Looking up and down stops just short of straight up and straight down. Esc brings the pointer back and the view stops following. |
| MOV-03 | Jump | 1. Press Space while standing.<br>2. Press Space again while in the air. | The character jumps and lands. The second press does nothing until it is back on the ground. |
| MOV-04 | Solid world | 1. Walk into a wall, then into a cheese block. | The character stops at walls and blocks and can't pass through them or leave the arena. |
| MOV-05 | Own body hidden | 1. Look straight down and around. | You don't see your own character's body or eyes, only your weapon and the hand holding it at the lower right, which bobs as you walk and kicks back when you fire. |
| MOV-06 | Fast mouse look | 1. Click in the game.<br>2. For half a minute, sweep the mouse quickly from side to side and in circles, as in a firefight. | The view always follows the mouse smoothly. It never snaps to an angle the mouse didn't move it to. |

## Other players

Setup: two windows. Why manual: this checks what one player sees of another across the connection.

| ID | Test | Steps | Expected |
|---|---|---|---|
| PLR-01 | Appearance | 1. Host in A, join in B.<br>2. Turn each view until the other character is visible. | Each sees the other as an upright wedge of cheese with holes, two eyes, two legs and two arms, holding their weapon. The two characters have different colours. |
| PLR-06 | Animation | 1. In B, walk, run in circles, jump and fire while A watches; then let A knock B out. | B's legs and arms swing as B walks, faster when moving faster, and stop when B stops. B's legs tuck up in a jump, and B's weapon kicks back with each shot. Knocked out, B sags and spreads into a puddle, then vanishes; on coming back B pops up from the floor. |
| PLR-02 | Movement is shared | 1. In B, walk and jump while watching window A. | A sees B's character move and jump as B does, smoothly, without stutters, freezes or jumps in position. |
| PLR-08 | Hitting a moving player from far away | Two devices on different networks with a noticeable ping (see the scoreboard), such as one on mobile data | 1. On each in turn, aim the Cheddar Blaster at the other while they run sideways, and fire. | Shots that look on target land, without needing to aim ahead of the runner, up to a ping of about 200 ms. Beyond that, a little lead is needed. |
| PLR-07 | Smooth on a poor connection | 1. In B's developer tools, set network throttling to "Slow 4G".<br>2. Run PLR-02 and PLR-03 again, both ways. | Movement still looks smooth in both windows, a little behind, with no stutter. Coming back into play after a knock-out still shows the player at the new spot at once, without sliding across the arena. |
| PLR-03 | Facing is shared | 1. In B, turn left and right, then look up and down, while watching window A. | B's eyes and weapon turn and tilt to match where B is looking. |
| PLR-04 | Starting spots | 1. Host in A, join in B.<br>2. Before moving, look at where each character is. | The two characters start in different places, each facing the middle of the arena. |
| PLR-05 | Name tags | 1. Host in A, join in B, each with a different name.<br>2. Turn each view until the other character is visible, then walk away from it. | A name floats above the other character, readable from near and far and through walls, and turns to face you. Your own name is not shown over your own head. |

## Combat

Setup: two windows. Why manual: these check what the fighting looks and feels like; the automated tests check that hits, damage and score are right.

| ID | Test | Steps | Expected |
|---|---|---|---|
| CMB-01 | The weapon in view | 1. Host in A, join in B.<br>2. In each, look at your own view, then at the other character. | Your own weapon shows at the lower right of your view and moves with it. The other character holds theirs in front of them. |
| CMB-02 | Holding the trigger | 1. In A, click to take the mouse, then hold the left button for a few seconds while looking around. | Shots keep coming at a steady rate while the button is held, and stop when it is released. The view never jumps when you click or hold. |
| CMB-04 | Shots are seen | 1. Host in A, join in B, and face each other.<br>2. Fire in A, then in B. | Each shot shows a brief flash at the end of the weapon and a thin yellow line to where it stopped, in both windows, whoever fired it. A line stops at the wall or player it hit. |
| CMB-05 | Crosshair and hit marker | 1. In A, fire at B's body, then at B's head, then until B is knocked out. | A small cross stays in the middle of A's view. Each hit flashes four short marks around it: white for the body, yellow for the head, red for the knock-out. A shot that misses shows no marks. |
| CMB-06 | Where the damage came from | 1. In B, turn so A is to your left, and let A shoot you. Repeat with A behind you and to your right. | A red curve appears at the edge of B's view on the side A is, fading over about a second. |
| CMB-07 | Health and ammo | 1. In A, watch the lower corners while firing, reloading and being shot by B. | Lower left: a health bar and number that drop when hit and turn red at 30 or less. Lower right: rounds left out of 24, which reads "Reloading…" while reloading. |
| CMB-08 | Kill feed | 1. Host in A, join in B, and knock B out from A. | In both windows a line appears at the top right, "A's name [Cheddar Blaster] B's name" (with "head" if the last hit was to the head), and goes after a few seconds. Several knock-outs in a row stack up, newest at the bottom. |
| CMB-09 | Scoreboard | 1. After CMB-08, hold Tab in each window, then let go. | While Tab is held, a table in the middle lists both players with their knock-outs and times out, best first, your own row in yellow. It goes when Tab is released. |
| CMB-13 | Pickups | 1. Host in A, join in B, and look around the arena.<br>2. In A, walk over each pad, then get hurt by B and walk over a wedge. | Each pad shows a slowly turning weapon, or a glowing cheese wedge, over a round base. Walking over one takes it: the item disappears for everyone, a note at the lower right says what you got, and a new weapon comes straight into your hands. A wedge only goes to a player who is hurt. Items come back after 15 seconds (wedges) or 20 (weapons). |
| CMB-10 | The weapons | Setup: a session where you have picked up every weapon (CMB-13). 1. Fire each at a wall and at the other player, near and far. | Each looks different in hand. The Swiss Shotgun sprays a cone of pellets, deadly up close and weak far off. The Parmesan Grater sprays fast and a little wide. The Gouda Launcher lobs a spinning wheel in a gentle arc that bursts in a growing flash on whatever it touches, or after about three seconds. |
| CMB-11 | Shoved by a burst | 1. Fire the Gouda Launcher at the floor just ahead of you while jumping. 2. Fire it at the other player's feet. | A burst throws players near it away and upwards, your own included, but only hurts others. A burst behind a wall from a player does nothing to them. |
| CMB-12 | Changing weapons | 1. With several weapons, press 1 to 4, then turn the mouse wheel both ways. | Each changes the weapon in hand, shown at the lower right with its ammunition, after a short moment before it can fire. The other window sees the new weapon in your hands. |
| CMB-14 | Gouda Overload | 1. Host in A, join in B. In A, knock B out twice, watching the lower left.<br>2. Press Q in A and shoot B. | A bar over A's health fills as A does damage, reading "Overload N%", then "Overload ready: Q". Q sets it off: the bar turns orange and counts down eight seconds, A glows orange in B's view and runs noticeably faster, and B, when hit, sags, darkens and slows for two seconds. Afterwards everything is back to normal. |
| CMB-03 | Knocked out and back | 1. In A, shoot B until B is knocked out, watching both windows. | In A, B's character disappears and you can walk through where it stood. In B, the view stops responding to movement and firing, and a notice in the middle says "Knocked out by" A's name and counts down to coming back. A few seconds later B is back, at a spot far from A, and both can see each other again. |

## Matches

Setup: two windows. Why manual: these check how a match reads on screen; the automated tests check the rules.

| ID | Test | Steps | Expected |
|---|---|---|---|
| MAT-01 | Match settings on the menu | 1. Press each of the four match buttons above "Host a session" a few times, then reload the page. | Each steps through its choices (mode, player limit, kill limit, minutes) and comes back round. After the reload they show what you last chose. |
| MAT-02 | The clock | 1. Host a 3-minute free-for-all in A, join in B, and watch the top of the screen. | The top middle reads "First to N · m:ss", counting down the same in both windows. |
| MAT-03 | The end of a match | 1. Host "First to 5" in A, join in B, and knock B out five times. | When the fifth knock-out lands, both windows show A's name and "wins!" in the middle above the scoreboard, with "Next match in" counting down from 10. No shot does damage during it. Then everyone is back in play, spread out, with scores at zero. |
| MAT-04 | Teams on screen | 1. Host "Cheddar vs Brie" in A, join in B and in a third window C. | Players are orange (Cheddar) or blue (Brie), with an orange wedge or a blue round badge over their heads and names in the same colour. The top of the screen shows both teams' scores. Shooting a teammate does nothing. |
| MAT-06 | Cheddar Canyon | 1. Host on Cheddar Canyon and walk the whole arena: up each ramp onto both mesas, along them, round and through the ridges in the middle, and behind the cheese blocks. | Every ramp can be walked up and down without getting stuck or bouncing, the mesas give a view over the middle, the walls are too high to get over even with a Gouda Launcher burst underfoot, and nowhere traps a player. Do it as a joiner too, with Overloads and bursts: you are never snapped back to where you were a moment before. |
| MAT-07 | Choosing the map | 1. Press the map button on the menu to choose Brie Box, host in A, and join in B.<br>2. Leave, choose Cheddar Canyon, and host again. | Both windows play on the chosen arena; B never falls or starts inside something on joining. The session list shows the map. Behind the menu is a view over the arena last played. |
| MAT-05 | The in-game menu | 1. In a session, press Esc until the menu shows, then Esc again.<br>2. Click Menu at the top left, then Resume.<br>3. Open it again and click "Leave session". | The first Esc only frees the mouse; the next opens the menu and the one after closes it. Menu opens it too. While it's open your character doesn't move, the game carries on for everyone else, and clicking Resume doesn't fire or take the mouse. Leaving returns to the main menu. |

## Sound

Setup: two windows, with sound on and headphones for SND-02. Why manual: these are about what is heard.

| ID | Test | Steps | Expected |
|---|---|---|---|
| SND-01 | Music and the first click | 1. Open the game and wait without clicking.<br>2. Click anywhere on the menu. | Nothing is heard until the click (browsers don't allow it before), then a cheerful tune loops behind the menu. It gets quieter once in a session and comes back up on returning to the menu. |
| SND-02 | Where sounds come from | 1. Host in A, join in B. In B, turn so A is to your left, then behind you, while A fires and walks. | A's shots and footsteps are heard from A's direction, louder when closer. |
| SND-03 | Every sound | 1. In a session, fire each weapon, reload, jump, walk, pick something up, set off an Overload, hit and be hit, and be knocked out. | Each has its own sound: a zap for the Blaster, a boom for the Shotgun, a rattle for the Grater, a thump and then a rumbling burst for the Launcher, clicks for reloading, a tick when your shot hits, a thud when you're hurt, a falling tone when knocked out, a chime for pickups and a rising whoosh for the Overload. None crackles or clips. |

## Input devices

Why manual: these need physical hardware.

| ID | Test | Setup | Steps | Expected |
|---|---|---|---|---|
| INP-01 | Gamepad | Two windows, a controller connected to the PC | 1. Enter a session and press any controller button.<br>2. Move the left stick.<br>3. Move the right stick.<br>4. Press the bottom face button (A on Xbox, B on Nintendo, Cross on PlayStation). | The left stick walks and the right stick looks, both with no drift when the sticks are at rest. The button jumps. |
| INP-08 | Gamepad: fire and reload | As INP-01 | 1. Hold the right trigger.<br>2. Fire a few shots, then press the left face button (X on Xbox, Y on Nintendo, Square on PlayStation). | The trigger fires while held. The button reloads. |
| INP-15 | Gamepad: changing weapons | As INP-01, carrying several weapons | 1. Press the right shoulder button, then the left. | Each steps to the next or the previous weapon. |
| INP-17 | Gamepad: Overload | As INP-01, with a full Overload meter | 1. Press the top face button (Y on Xbox, X on Nintendo, Triangle on PlayStation). | The Overload starts. |
| INP-11 | Gamepad: scoreboard | As INP-01 | 1. Hold the Back (View, Select, Share or minus) button. | The scoreboard shows while it is held. |
| INP-13 | Gamepad: menu | As INP-01 | 1. Press Start (Menu, Options or plus).<br>2. Press it again. | The in-game menu opens, then closes. |
| INP-02 | Gamepad types | As INP-01, repeated with each controller you have: Xbox, PlayStation, Nintendo | Repeat INP-01. | Each controller behaves the same way. |
| INP-03 | Touch: move | Phone and PC; join or host on the phone | 1. Put a thumb on the left half of the screen and drag. | The character walks in the direction of the drag, faster the further the thumb is from where it first touched, and stops when the thumb lifts. |
| INP-04 | Touch: look | As INP-03 | 1. Drag on the right half of the screen. | The view follows the drag. |
| INP-05 | Touch: jump | As INP-03 | 1. Tap the right half of the screen quickly. | The character jumps. A slow drag on the right half does not jump. |
| INP-06 | Touch: both thumbs | As INP-03 | 1. Walk with the left thumb while looking with the right. | Both work at once and neither interrupts the other. |
| INP-09 | Touch: fire and reload | As INP-03 | 1. Touch the screen once. Two round buttons, FIRE and R, appear at the lower right.<br>2. Hold FIRE, then lift.<br>3. Tap R.<br>4. Hold FIRE while dragging on the right half to look. | FIRE fires while held and stops when lifted. R reloads. Touching a button doesn't turn the view or jump, and looking still works while FIRE is held. |
| INP-16 | Touch: changing weapons | As INP-03, carrying several weapons | 1. Tap the round Swap button by FIRE. | Each tap steps to the next weapon, without turning the view. |
| INP-18 | Touch: Overload | As INP-03, with a full Overload meter | 1. Tap the round GO button above FIRE. | The Overload starts, without turning the view. |
| INP-12 | Touch: scoreboard | As INP-03 | 1. Hold the round Scores button at the top middle of the screen, then lift. | The scoreboard shows while it is held, and holding it doesn't turn the view. |
| INP-14 | Touch: in-game menu | As INP-03 | 1. Tap Menu at the top left.<br>2. Tap Resume. | The menu opens and closes. Neither tap turns the view or makes the character jump. |
| INP-07 | Touch: menu | Phone and PC | 1. On the phone, use the menu to host, then reload and join by tapping a listed session and by typing a code. | Every button and the code box respond to taps, including the match settings buttons, and the on-screen keyboard appears for the name and code boxes. |

## Background tabs and screen sleep

Setup: two windows unless stated. Why manual: these depend on how each real browser treats a hidden tab, which the test harness can only imitate.

| ID | Test | Steps | Expected |
|---|---|---|---|
| BG-01 | Hidden host keeps the session running | 1. Host in A, join in B.<br>2. In A's window, switch to another tab.<br>3. In B, walk around for 30 seconds. | B can move and look the whole time. "Waiting for host…" never appears. |
| BG-02 | Minimized host | 1. As BG-01, but minimize A's window. | As BG-01. |
| BG-03 | Hidden joiner catches up (three windows) | 1. Host in A, join in B and C.<br>2. Hide B's tab.<br>3. Move C's character, then close window C.<br>4. Bring B's tab back. | B shows the session as it now is: C's character gone and "2 playing". B can move at once. |
| BG-04 | Returning to a hidden tab | 1. Host in A, join in B.<br>2. Hide A's tab for a minute, then bring it back. | A's view is live at once, showing B where B now is. |
| BG-05 | Screen stays awake | Setup: one window at `http://localhost:8060` on a laptop or tablet with a short screen timeout.<br>1. Host a session and don't touch the device for longer than the timeout. | The screen stays on while the session is open and visible. |
| BG-06 | Screen sleeps again afterwards | 1. After BG-05, close the tab and leave the device untouched. | The screen dims and locks as usual. |

The screen wake lock only exists on HTTPS and `localhost`, so BG-05 can't pass on a phone that reaches the game at `http://<PC address>:8060`.

## Losing the host

Why manual: these depend on real connection loss, which behaves differently from the simulated version.

| ID | Test | Setup | Steps | Expected |
|---|---|---|---|---|
| LOST-04 | Phone host puts the browser away | Phone and PC; host on the phone, join on the PC | 1. On the phone, switch to another app. | Within about two seconds the PC shows "Waiting for host…" with a moving spinner. |
| LOST-05 | Frozen while waiting | As LOST-04 | 1. While the notice is showing, press movement keys and move the mouse. | The character and the view don't move. The mouse pointer is visible and free. |
| LOST-06 | Host comes back | As LOST-04 | 1. Within a few seconds, switch back to the browser on the phone. | The notice disappears on the PC and both players can move again. Clicking in the game captures the mouse as before. |
| LOST-07 | Leave while waiting | As LOST-04 | 1. While the notice is showing, click "Leave". | The PC returns to the menu with no "Disconnected" notice. |
| LOST-08 | Host loses its network | Phone and PC; host on the phone, join on the PC | 1. Turn off Wi-Fi on the phone and leave it off. | The PC shows "Waiting for host…" within about two seconds. Within about a minute it shows "Disconnected". |
| LOST-10 | Rejoining after a dropped connection | Phone and PC; host on the PC, join on the phone | 1. Knock the phone's player out once.<br>2. Turn off Wi-Fi on the phone until it shows "Disconnected", then turn it back on.<br>3. Tap Rejoin. | The notice offers Rejoin (it doesn't when the host closed the session). Rejoining puts the phone back in the same session, and the scoreboard still counts the earlier knock-out against it. |
| LOST-09 | Joining a host that is away | Phone and PC; host on the phone | 1. Switch the phone to another app.<br>2. On the PC, refresh the list and join the session. | The PC shows "Joining…" and, after about 30 seconds, returns to the menu with "Timed out connecting to the host." |

## Load

Why manual: these need many real devices, and measure the host's machine.

| ID | Test | Setup | Steps | Expected |
|---|---|---|---|---|
| LOAD-01 | A full session | 16 browser windows across several PCs and phones, one hosting on a phone | 1. Join everyone to one session and fight for five minutes, with the host's browser task manager (or the phone's developer tools) open. | Everyone stays connected and sees everyone else move smoothly. The host keeps a steady frame rate and its network upload stays under about 1 MB/s. The scoreboard's Ping column shows each player's round trip, "host" for the host. |

## Phones, browsers and networks

Why manual: each combination is a different real device or program.

| ID | Test | Setup | Steps | Expected |
|---|---|---|---|---|
| DEV-01 | Loads on a phone | Phone and PC | 1. Open the game on the phone. | The game loads to the menu. The menu is large enough to read and tap, in portrait and in landscape. |
| DEV-02 | Phone joins a PC host | Phone and PC | 1. Host on the PC, join on the phone. | The phone enters the arena and each sees the other move. |
| DEV-03 | Phone hosts | Phone and PC | 1. Host on the phone, join on the PC. | As DEV-02. |
| DEV-04 | Each browser | Two windows of the browser under test | In each of Chrome, Firefox, Edge and Safari:<br>1. Host a session in one window and join it from the session list in the other.<br>2. Run MOV-01, MOV-02 and PLR-02. | Joining works and the tests pass in every browser. |
| DEV-05 | Mixed browsers | One window each of two different browsers | 1. Host in one, join in the other, then swap. | Both directions connect and PLR-02 passes. |
| DEV-06 | Different networks | Two devices on different internet connections, such as home Wi-Fi and mobile data | 1. Host on one, join on the other. | They connect and PLR-02 passes. |

DEV-06 can't be run yet: the signaling server only runs on the development PC, so a device on another network can't reach it. It becomes runnable once the server is hosted publicly.

## Native build

Setup: native pair. Why manual: this is the direct-connection path used for development, checked through the real window.

| ID | Test | Steps | Expected |
|---|---|---|---|
| NAT-01 | Host and join from the command line | 1. Start the host instance, then the joining instance. | Both open straight into the arena without showing the menu, and each sees the other's character. |
| NAT-02 | Host and join from the menu | 1. Start two instances with `godot --path game`.<br>2. Click "Host a session" in one.<br>3. In the other, leave the address box empty and click "Join". | The menu shows an address box in place of the code box, and no session list. The second instance joins the first. |
| NAT-03 | Port already in use | 1. With one instance hosting, click "Host a session" in a second. | The second stays on the menu, showing "Couldn't open port 7777." |
| NAT-04 | Movement | 1. Run MOV-01 to MOV-04 in a native instance. | All pass. |
