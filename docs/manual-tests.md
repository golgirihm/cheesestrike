# Manual tests

Behavior that the automated tests don't cover and that has to be checked by a person. The automated suites (`python tools/run_tests.py`) cover the rules for detecting a lost host and the signaling server's protocol; everything a player sees, touches or does across a real browser connection is here.

Some of these could be automated later with a browser automation tool. They are listed here because that isn't set up, not because it can't be done.

**Keeping this current:** when a change adds or alters behavior that an automated test can't cover, add or update a row here in the same pull request. When a row becomes covered by an automated test, delete it.

**What to run:** the sections for whatever a change touches. Run everything before sharing a build with other people.

## Setups

Each section names the setup it needs. Build and serve the web version first, as described in the [README](../README.md#development).

| Setup | What it is |
|---|---|
| **Two windows** | Two browser windows side by side on one PC, both at `http://localhost:8060`. One hosts, the other joins. Both stay visible unless a step says otherwise. |
| **Three windows** | As above, with a third window that also joins. |
| **Phone and PC** | A phone on the same Wi-Fi as the PC, at `http://<PC address>:8060`, and one browser window on the PC. |
| **Native pair** | Two native instances on one PC: `godot --path game -- --host` and `godot --path game -- --join=127.0.0.1`. |

## Sessions

Setup: two windows. Why manual: these go through the real menu, a real signaling connection and a real WebRTC connection between browsers.

| ID | Test | Steps | Expected |
|---|---|---|---|
| SES-01 | Host a session | 1. Click "Host a session". | The menu disappears and you are standing in the arena. The top-left reads "Session ABCD · 1 playing", with a four-letter code. |
| SES-02 | Join from the list | 1. Host in window A.<br>2. In window B, click "Refresh".<br>3. Click the session in the list. | B enters the arena within a few seconds. Both windows read "2 playing". |
| SES-03 | Join by code | 1. Host in window A and note the code.<br>2. In window B, type the code in lower case and press Enter. | B joins, as in SES-02. The code is accepted in any letter case. |
| SES-04 | Session list contents | 1. With no sessions hosted, open the menu.<br>2. Host in window A.<br>3. In window B, click "Refresh".<br>4. Close window A, then click "Refresh" in B. | Step 1: "None yet. Host one!"<br>Step 3: the session appears as "ABCD · 1 playing".<br>Step 4: the list is empty again. |
| SES-05 | Empty code | 1. Leave the code box empty and click "Join". | "Enter a session code." appears and nothing else happens. |
| SES-06 | Unknown code | 1. Type a code that isn't hosted, such as `ZZZZ`, and click "Join". | The menu stays, showing "No session with that code." The buttons work again. |
| SES-07 | Session server not running | 1. Stop the signaling server.<br>2. Reload the page and click "Host a session". | The menu stays, showing "Can't reach the session server at ws://…". Starting the server and trying again works without reloading. |
| SES-08 | A joiner leaves | 1. Host in A, join in B.<br>2. Close window B. | Within a few seconds A reads "1 playing" and B's character is gone. |
| SES-09 | Late joiner (three windows) | 1. Host in A, join in B, and move both characters away from their starting spots.<br>2. Join in C. | C sees both existing characters where they currently are. A and B see C appear. All read "3 playing". |

## Movement and view

Setup: two windows; do the steps in either one. Why manual: this is how the controls feel and what the screen shows.

| ID | Test | Steps | Expected |
|---|---|---|---|
| MOV-01 | Walk | 1. Click in the game.<br>2. Hold W, A, S, D in turn, then the arrow keys. | The character moves forward, left, back and right relative to where it is facing, at a steady speed, and stops when the key is released. |
| MOV-02 | Look with the mouse | 1. Click in the game.<br>2. Move the mouse in all directions.<br>3. Press Esc. | After the click the pointer disappears and the view follows the mouse. Looking up and down stops just short of straight up and straight down. Esc brings the pointer back and the view stops following. |
| MOV-03 | Jump | 1. Press Space while standing.<br>2. Press Space again while in the air. | The character jumps and lands. The second press does nothing until it is back on the ground. |
| MOV-04 | Solid world | 1. Walk into a wall, then into a cheese block. | The character stops at walls and blocks and can't pass through them or leave the arena. |
| MOV-05 | Own body hidden | 1. Look straight down and around. | You don't see your own character's body or visor. |
| MOV-06 | Fast mouse look | 1. Click in the game.<br>2. For half a minute, sweep the mouse quickly from side to side and in circles, as in a firefight. | The view always follows the mouse smoothly. It never snaps to an angle the mouse didn't move it to. |

## Other players

Setup: two windows. Why manual: this checks what one player sees of another across the connection.

| ID | Test | Steps | Expected |
|---|---|---|---|
| PLR-01 | Appearance | 1. Host in A, join in B.<br>2. Turn each view until the other character is visible. | Each sees the other as a colored capsule with a dark visor. The two characters have different colors. |
| PLR-02 | Movement is shared | 1. In B, walk and jump while watching window A. | A sees B's character move and jump as B does, without long freezes or jumps in position. |
| PLR-03 | Facing is shared | 1. In B, turn left and right, then look up and down, while watching window A. | The visor on B's character turns and tilts to match where B is looking. |
| PLR-04 | Starting spots | 1. Host in A, join in B.<br>2. Before moving, look at where each character is. | The two characters start in different places, each facing the middle of the arena. |

## Input devices

Why manual: these need physical hardware.

| ID | Test | Setup | Steps | Expected |
|---|---|---|---|---|
| INP-01 | Gamepad | Two windows, a controller connected to the PC | 1. Enter a session and press any controller button.<br>2. Move the left stick.<br>3. Move the right stick.<br>4. Press the bottom face button (A on Xbox, B on Nintendo, Cross on PlayStation). | The left stick walks and the right stick looks, both with no drift when the sticks are at rest. The button jumps. |
| INP-02 | Gamepad types | As INP-01, repeated with each controller you have: Xbox, PlayStation, Nintendo | Repeat INP-01. | Each controller behaves the same way. |
| INP-03 | Touch: move | Phone and PC; join or host on the phone | 1. Put a thumb on the left half of the screen and drag. | The character walks in the direction of the drag, faster the further the thumb is from where it first touched, and stops when the thumb lifts. |
| INP-04 | Touch: look | As INP-03 | 1. Drag on the right half of the screen. | The view follows the drag. |
| INP-05 | Touch: jump | As INP-03 | 1. Tap the right half of the screen quickly. | The character jumps. A slow drag on the right half does not jump. |
| INP-06 | Touch: both thumbs | As INP-03 | 1. Walk with the left thumb while looking with the right. | Both work at once and neither interrupts the other. |
| INP-07 | Touch: menu | Phone and PC | 1. On the phone, use the menu to host, then reload and join by tapping a listed session and by typing a code. | Every button and the code box respond to taps, and the on-screen keyboard appears for the code box. |

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
| LOST-01 | Host closes the page | Two windows | 1. Host in A, join in B.<br>2. Close window A. | B may show "Waiting for host…" briefly. Within a few seconds B shows "Disconnected" with "The host closed the session." |
| LOST-02 | Back to menu | Two windows | 1. After LOST-01, click "Back to menu" in B.<br>2. Host a new session from B. | The menu returns with no message left over, and hosting works. |
| LOST-03 | Every joiner is told | Three windows | 1. Host in A, join in B and C.<br>2. Close window A. | B and C both show "Disconnected" within a few seconds of each other. |
| LOST-04 | Phone host puts the browser away | Phone and PC; host on the phone, join on the PC | 1. On the phone, switch to another app. | Within about two seconds the PC shows "Waiting for host…" with a moving spinner. |
| LOST-05 | Frozen while waiting | As LOST-04 | 1. While the notice is showing, press movement keys and move the mouse. | The character and the view don't move. The mouse pointer is visible and free. |
| LOST-06 | Host comes back | As LOST-04 | 1. Within a few seconds, switch back to the browser on the phone. | The notice disappears on the PC and both players can move again. Clicking in the game captures the mouse as before. |
| LOST-07 | Leave while waiting | As LOST-04 | 1. While the notice is showing, click "Leave". | The PC returns to the menu with no "Disconnected" notice. |
| LOST-08 | Host loses its network | Phone and PC; host on the phone, join on the PC | 1. Turn off Wi-Fi on the phone and leave it off. | The PC shows "Waiting for host…" within about two seconds. Within about a minute it shows "Disconnected". |
| LOST-09 | Joining a host that is away | Phone and PC; host on the phone | 1. Switch the phone to another app.<br>2. On the PC, refresh the list and join the session. | The PC shows "Joining…" and, after about 30 seconds, returns to the menu with "Timed out connecting to the host." |

## Phones, browsers and networks

Why manual: each combination is a different real device or program.

| ID | Test | Setup | Steps | Expected |
|---|---|---|---|---|
| DEV-01 | Loads on a phone | Phone and PC | 1. Open the game on the phone. | The game loads to the menu. The menu is large enough to read and tap, in portrait and in landscape. |
| DEV-02 | Phone joins a PC host | Phone and PC | 1. Host on the PC, join on the phone. | The phone enters the arena and each sees the other move. |
| DEV-03 | Phone hosts | Phone and PC | 1. Host on the phone, join on the PC. | As DEV-02. |
| DEV-04 | Each browser | Two windows of the browser under test | 1. Run SES-01, SES-02, MOV-01, MOV-02 and PLR-02 in Chrome, Firefox, Edge and Safari. | All pass in every browser. |
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
